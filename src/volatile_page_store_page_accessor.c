#include<volatile_page_store_page_accessor.h>

#include<block_io_mmap_wrapper.h>

#include<system_page_header_util.h>

#include<bitmap.h>

#include<stdlib.h>
#include<stdio.h>

static void* allocate_from_free_list(volatile_page_store* vps, uint64_t* page_id)
{
	// no pages in free list
	if(vps->free_pages_list_head_page_id == vps->user_stats.NULL_PAGE_ID)
		return NULL;

	// set the page_id to be returned
	(*page_id) = vps->free_pages_list_head_page_id;

	// get the page
	void* page = block_io_get_page(vps, (*page_id));

	// remove page from the head of the free_pages_list
	vps->free_pages_list_head_page_id = get_page_id_for_page(page, &(vps->stats));

	return page;
}

static void* allocate_from_free_space_map(volatile_page_store* vps, uint64_t* page_id)
{
	// we are calling a free spacemapper page and group of pages following it an extent for the context of this function
	const uint64_t data_pages_per_extent = is_valid_bits_count_on_free_space_mapper_page(&(vps->stats));
	const uint64_t total_pages_per_extent = data_pages_per_extent + 1;

	uint64_t free_space_mapper_page_id = 0;
	while(free_space_mapper_page_id < vps->active_page_count)
	{
		{
			// get free space mapper page that we interested in
			void* free_space_mapper_page = block_io_get_page(vps, free_space_mapper_page_id);

			uint64_t free_space_mapper_bit_index = 0;
			while(free_space_mapper_bit_index < data_pages_per_extent)
			{
				// calculate respective page_id, and ensure that it does not overflow
				if(will_unsigned_sum_overflow(uint64_t, free_space_mapper_page_id, (free_space_mapper_bit_index + 1)))
					break;
				(*page_id) = free_space_mapper_page_id + (free_space_mapper_bit_index + 1);
				if((*page_id) >= vps->active_page_count)
					break;

				// if the free_space_mapper_bit_index is set, continue
				void* free_space_mapper_page_contents = get_page_contents_for_page(free_space_mapper_page, free_space_mapper_page_id, &(vps->stats));
				if(get_bit(free_space_mapper_page_contents, free_space_mapper_bit_index))
				{
					free_space_mapper_bit_index++;
					continue;
				}

				// else we allocate it, by just setting the bit
				set_bit(free_space_mapper_page_contents, free_space_mapper_bit_index);

				// release free space mapper page
				block_io_return_page(vps, free_space_mapper_page);

				// grab the page we just allocated and return
				return block_io_get_page(vps, (*page_id));
			}

			// return free space mapper page back using munmap
			block_io_return_page(vps, free_space_mapper_page);
		}

		// check for overflow and increment
		if(will_unsigned_sum_overflow(uint64_t, free_space_mapper_page_id, total_pages_per_extent))
			break;
		free_space_mapper_page_id += total_pages_per_extent;
	}

	return NULL;
}

static void* allocate_by_extending_file(volatile_page_store* vps, uint64_t* page_id)
{
	// if out of pages, fail
	if(vps->active_page_count == vps->user_stats.max_page_count)
		return NULL;

	void* free_space_mapper_page = NULL;
	uint64_t free_space_mapper_page_id;

	void* page = NULL;

	if(!is_free_space_mapper_page(vps->active_page_count, &(vps->stats)))
	{
		(*page_id) = vps->active_page_count;

		// first grab latch on the free space mapper page
		free_space_mapper_page_id = get_is_valid_bit_page_id_for_page((*page_id), &(vps->stats));
		free_space_mapper_page = block_io_get_page(vps, free_space_mapper_page_id);

		page = block_io_get_new_page(vps, page_id);
	}
	else // if next new page is a free space mapper page, then create 2 instead of 1
	{
		// make sure that you can add 2 new pages
		if(vps->user_stats.max_page_count - vps->active_page_count < 2)
			return NULL;

		free_space_mapper_page = block_io_get_new_page(vps, &free_space_mapper_page_id);

		page = block_io_get_new_page(vps, page_id);
	}

	// perform the actual allocation
	{
		uint64_t free_space_mapper_bit_pos = get_is_valid_bit_position_for_page((*page_id), &(vps->stats));
		void* free_space_mapper_page_contents = get_page_contents_for_page(free_space_mapper_page, free_space_mapper_page_id, &(vps->stats));
		if(get_bit(free_space_mapper_page_contents, free_space_mapper_bit_pos) != 0) // this may never happen, we are just ensuring that the page being allocated is free
		{
			printf("ISSUEv :: bug in page allocation or new page initialization, page attempting to be allocated is not marked free\n");
			exit(-1);
		}
		set_bit(free_space_mapper_page_contents, free_space_mapper_bit_pos);
	}

	// return free space mapper page back using munmap
	block_io_return_page(vps, free_space_mapper_page);

	return page;
}

void* get_new_page_for_vps(volatile_page_store* vps, uint64_t* page_id)
{
	void* page = NULL;

	pthread_mutex_lock(&(vps->manager_lock));

	{
		// strategy 1 :: allocate form free list
		page = allocate_from_free_list(vps, page_id);
		if(page != NULL)
			goto EXIT;

		// strategy 2 :: allocate a new one from the free space map
		page = allocate_from_free_space_map(vps, page_id);
		if(page != NULL)
			goto EXIT;

		// strategy 3 :: extend the file and allocate a brand new one
		page = allocate_by_extending_file(vps, page_id);
		if(page != NULL)
			goto EXIT;
	}

	EXIT:;
	if(page == NULL)
	{
		printf("ISSUEv :: could not allocate new page for the user\n");
		exit(-1);
	}
	pthread_mutex_unlock(&(vps->manager_lock));

	// write page_id on the page, so we do not have to remember this mapping
	set_page_id_for_page(page, (*page_id), &(vps->stats));

	// return the page contents for this page
	return get_page_contents_for_page(page, (*page_id), &(vps->stats));
}

void* acquire_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// page_id must not be a free space mapper page
	if(is_free_space_mapper_page(page_id, &(vps->stats)))
	{
		printf("ISSUEv :: user accessing free space mapper page\n");
		exit(-1);
	}

	// get the page
	void* page = block_io_get_page(vps, page_id);

	// write page_id on the page, so we do not have to remember this mapping
	set_page_id_for_page(page, page_id, &(vps->stats));

	// return the page contents for this page
	return get_page_contents_for_page(page, page_id, &(vps->stats));
}

void release_page_for_vs(volatile_page_store* vps, void* page_contents, int free_page)
{
	// get page from the page contents
	void* page = page_contents - get_system_header_size_for_data_pages(&(vps->stats));

	if(!free_page)
		block_io_return_page(vps, page);
	else
	{
		// grab page id of the page
		uint64_t page_id = get_page_id_for_page(page, &(vps->stats));

		// put this page in the head of the free_pages_list
		{
			pthread_mutex_lock(&(vps->manager_lock));

				set_page_id_for_page(page, vps->free_pages_list_head_page_id, &(vps->stats));

				vps->free_pages_list_head_page_id = page_id;

				block_io_return_page(vps, page);

			pthread_mutex_unlock(&(vps->manager_lock));
		}
	}
}

void free_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// page_id must not be a free space mapper page
	if(is_free_space_mapper_page(page_id, &(vps->stats)))
	{
		printf("ISSUEv :: user freeing free space mapper page\n");
		exit(-1);
	}

	// get the page
	void* page = block_io_get_page(vps, page_id);

	// put this page in the head of the free_pages_list
	{
		pthread_mutex_lock(&(vps->manager_lock));

			set_page_id_for_page(page, vps->free_pages_list_head_page_id, &(vps->stats));

			vps->free_pages_list_head_page_id = page_id;

			block_io_return_page(vps, page);

		pthread_mutex_unlock(&(vps->manager_lock));
	}
}