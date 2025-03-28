#include<volatile_page_store_page_accessor.h>

#include<mmaped_file_pool.h>

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
	void* page = acquire_page(&(vps->pool), (*page_id));

	// remove page from the head of the free_pages_list
	vps->free_pages_list_head_page_id = deserialize_uint64(page, vps->stats.page_id_width);

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
			void* free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);

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
				if(get_bit(free_space_mapper_page, free_space_mapper_bit_index))
				{
					free_space_mapper_bit_index++;
					continue;
				}

				// else we allocate it, by just setting the bit
				set_bit(free_space_mapper_page, free_space_mapper_bit_index);

				// release free space mapper page
				release_page(&(vps->pool), free_space_mapper_page);

				// grab the page we just allocated and return
				return acquire_page(&(vps->pool), (*page_id));
			}

			// return free space mapper page back using munmap
			release_page(&(vps->pool), free_space_mapper_page);
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
		free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);

		(*page_id) = ((vps->active_page_count)++);
		// expand the file
		{
			uint64_t block_count = (vps->active_page_count) * (vps->stats.page_size / get_block_size_for_block_file(&(vps->temp_file)));
			if(!truncate_block_file(&(vps->temp_file), block_count))
			{
				printf("ISSUEv :: could not expand the file\n");
				exit(-1);
			}
		}

		page = acquire_page(&(vps->pool), *(page_id));
	}
	else // if next new page is a free space mapper page, then create 2 instead of 1
	{
		// make sure that you can add 2 new pages
		if(vps->user_stats.max_page_count - vps->active_page_count < 2)
			return NULL;

		free_space_mapper_page_id = ((vps->active_page_count)++);
		(*page_id) = ((vps->active_page_count)++);
		// expand the file
		{
			uint64_t block_count = (vps->active_page_count) * (vps->stats.page_size / get_block_size_for_block_file(&(vps->temp_file)));
			if(!truncate_block_file(&(vps->temp_file), block_count))
			{
				printf("ISSUEv :: could not expand the file\n");
				exit(-1);
			}
		}

		free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);
		memory_set(free_space_mapper_page, 0, vps->stats.page_size);
		page = acquire_page(&(vps->pool), (*page_id));
	}

	// perform the actual allocation
	{
		uint64_t free_space_mapper_bit_pos = get_is_valid_bit_position_for_page((*page_id), &(vps->stats));
		if(get_bit(free_space_mapper_page, free_space_mapper_bit_pos) != 0) // this may never happen, we are just ensuring that the page being allocated is free
		{
			printf("ISSUEv :: bug in page allocation or new page initialization, page attempting to be allocated is not marked free\n");
			exit(-1);
		}
		set_bit(free_space_mapper_page, free_space_mapper_bit_pos);
	}

	// return free space mapper page back using munmap
	release_page(&(vps->pool), free_space_mapper_page);

	return page;
}

void* get_new_page_for_vps(volatile_page_store* vps, uint64_t* page_id)
{
	void* page = NULL;

	pthread_mutex_lock(&(vps->global_lock));

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
	pthread_mutex_unlock(&(vps->global_lock));

	return page;
}

void* acquire_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// page_id must not be a free space mapper page
	if(is_free_space_mapper_page(page_id, &(vps->stats)))
	{
		printf("ISSUEv :: user accessing free space mapper page\n");
		exit(-1);
	}

	pthread_mutex_lock(&(vps->global_lock));

		if(page_id >= vps->active_page_count)
		{
			printf("ISSUEv :: user accessing an out of bounds page_id\n");
			exit(-1);
		}

		// get the page
		void* page = acquire_page(&(vps->pool), page_id);

	pthread_mutex_unlock(&(vps->global_lock));

	return page;
}

void release_page_for_vps(volatile_page_store* vps, void* page, int free_page)
{
	pthread_mutex_lock(&(vps->global_lock));

	if(!free_page)
		release_page(&(vps->pool), page);
	else
	{
		// grab page id of the page
		uint64_t page_id = get_page_id_for_frame(&(vps->pool), page);
		if(page_id == UINT64_MAX)
		{
			printf("ISSUEv :: page to be released was never acquired\n");
			exit(-1);
		}

		// put this page in the head of the free_pages_list
		{
			// link this page into the free_pages_list
			serialize_uint64(page, vps->stats.page_id_width, vps->free_pages_list_head_page_id);
			vps->free_pages_list_head_page_id = page_id;

			release_page(&(vps->pool), page);
		}
	}

	pthread_mutex_unlock(&(vps->global_lock));
}

void free_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// page_id must not be a free space mapper page
	if(is_free_space_mapper_page(page_id, &(vps->stats)))
	{
		printf("ISSUEv :: user freeing free space mapper page\n");
		exit(-1);
	}

	// put this page in the head of the free_pages_list
	{
		pthread_mutex_lock(&(vps->global_lock));

			// get the page
			void* page = acquire_page(&(vps->pool), page_id);

			// link this page into the free_pages_list
			serialize_uint64(page, vps->stats.page_id_width, vps->free_pages_list_head_page_id);
			vps->free_pages_list_head_page_id = page_id;

			release_page(&(vps->pool), page);

		pthread_mutex_unlock(&(vps->global_lock));
	}
}