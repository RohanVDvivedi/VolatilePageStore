#include<volatilepagestore/volatile_page_store_page_accessor.h>

#include<volatilepagestore/mmaped_file_pool.h>

#include<volatilepagestore/system_page_header_util.h>

#include<volatilepagestore/volatile_page_store_free_space_management.h>

#include<cutlery/bitmap.h>

#include<stdlib.h>
#include<stdio.h>

static void* allocate_from_free_list(volatile_page_store* vps, uint64_t* page_id)
{
	// no pages in free list
	if(vps->free_pages_list_head_page_id == vps->user_stats.NULL_PAGE_ID)
		return NULL;

	// else remove and allocate the first page
	(*page_id) = vps->free_pages_list_head_page_id;
	mark_allocated_in_free_space_bitmap_page_vps(vps, (*page_id));
	return remove_from_free_pages_list_vps(vps, (*page_id));
}

static void* allocate_by_extending_file(volatile_page_store* vps, uint64_t* page_id)
{
	// if out of pages, fail
	if(vps->active_page_count == vps->user_stats.max_page_count)
		return NULL;

	if(!is_free_space_mapper_page_vps(vps->active_page_count, &(vps->stats)))
	{
		(*page_id) = (vps->active_page_count)++;

		// expand the file
		{
			uint64_t block_count = (vps->active_page_count) * (vps->stats.page_size / get_block_size_for_block_file(&(vps->temp_file)));
			if(!truncate_block_file(&(vps->temp_file), block_count))
			{
				printf("ISSUEv :: could not expand the file\n");
				exit(-1);
			}
		}

		mark_allocated_in_free_space_bitmap_page_vps(vps, (*page_id));

		return acquire_page(&(vps->pool), (*page_id));
	}
	else // if next new page is a free space mapper page, then create 2 instead of 1
	{
		// make sure that you can add 2 new pages
		if(vps->user_stats.max_page_count - vps->active_page_count < 2)
			return NULL;

		uint64_t free_space_mapper_page_id = (vps->active_page_count)++;
		(*page_id) = (vps->active_page_count)++;

		// expand the file
		{
			uint64_t block_count = (vps->active_page_count) * (vps->stats.page_size / get_block_size_for_block_file(&(vps->temp_file)));
			if(!truncate_block_file(&(vps->temp_file), block_count))
			{
				printf("ISSUEv :: could not expand the file\n");
				exit(-1);
			}
		}

		// must 0 initialize free space mapper page
		{
			void* free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);
			memory_set(free_space_mapper_page, 0, vps->stats.page_size);
			release_page(&(vps->pool), free_space_mapper_page);
		}

		mark_allocated_in_free_space_bitmap_page_vps(vps, (*page_id));

		return acquire_page(&(vps->pool), (*page_id));
	}
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

		// strategy 2 :: extend the file and allocate a brand new one
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
	if(is_free_space_mapper_page_vps(page_id, &(vps->stats)))
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
			insert_in_free_pages_list_vps(vps, page_id, page);

			mark_free_in_free_space_bitmap_page_vps(vps, page_id);
		}
	}

	pthread_mutex_unlock(&(vps->global_lock));
}

void free_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// page_id must not be a free space mapper page
	if(is_free_space_mapper_page_vps(page_id, &(vps->stats)))
	{
		printf("ISSUEv :: user freeing free space mapper page\n");
		exit(-1);
	}

	// put this page in the head of the free_pages_list
	{
		pthread_mutex_lock(&(vps->global_lock));

			// get the page
			void* page = acquire_page(&(vps->pool), page_id);

			insert_in_free_pages_list_vps(vps, page_id, page);

			mark_free_in_free_space_bitmap_page_vps(vps, page_id);

		pthread_mutex_unlock(&(vps->global_lock));
	}
}