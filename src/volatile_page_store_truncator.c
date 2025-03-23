#include<volatile_page_store_truncator.h>

#include<system_page_header_util.h>

#include<bitmap.h>

#include<stdio.h>
#include<errno.h>
#include<stdlib.h>
#include<pthread.h>

#include<pthread_cond_utils.h>

#include<volatile_page_store.h>
#include<mmaped_file_pool.h>

void perform_truncation(volatile_page_store* vps)
{
	{
		// preserving the free_space_mapper_page
		uint64_t free_space_mapper_page_id;
		void* free_space_mapper_page = NULL;

		// free all from free_pages_list
		while(vps->free_pages_list_head_page_id != vps->user_stats.NULL_PAGE_ID)
		{
			// grab the head's page_id and page
			uint64_t page_id = vps->free_pages_list_head_page_id;
			void* page = acquire_page(&(vps->pool), page_id);

			// update free_pages_list_head_page_id, effectively popping head
			vps->free_pages_list_head_page_id = deserialize_uint64(page, vps->stats.page_id_width);

			// return page back to OS
			release_page(&(vps->pool), page);

			// grab the free space mapper page and page_id, if not already mmaped
			if(free_space_mapper_page == NULL)
			{
				free_space_mapper_page_id = get_is_valid_bit_page_id_for_page(page_id, &(vps->stats));
				free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);
			}
			else
			{
				// we need to meddle only if the free_space_mapper_page that is mapped is not the right one
				if(free_space_mapper_page_id != get_is_valid_bit_page_id_for_page(page_id, &(vps->stats)))
				{
					// unmap the old one and map the new one
					release_page(&(vps->pool), free_space_mapper_page);

					free_space_mapper_page_id = get_is_valid_bit_page_id_for_page(page_id, &(vps->stats));
					free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);
				}
			}

			// mark the page_id as free in free space map
			{
				uint64_t free_space_mapper_bit_pos = get_is_valid_bit_position_for_page(page_id, &(vps->stats));
				if(get_bit(free_space_mapper_page, free_space_mapper_bit_pos) != 1) // this may never happen, we are just ensuring that the page being freed is indeed allocated
				{
					printf("ISSUEv :: bug in page being freed from free pages list, before truncation, page in free pages list is not marked allocated\n");
					exit(-1);
				}
				reset_bit(free_space_mapper_page, free_space_mapper_bit_pos);
			}
		}

		// if any free space mapper page is mmaped release it
		if(free_space_mapper_page != NULL)
			release_page(&(vps->pool), free_space_mapper_page);
	}

	if(vps->active_page_count > 0)
	{
		uint64_t new_active_page_count = vps->active_page_count;

		uint64_t block_count_per_page = vps->stats.page_size / get_block_size_for_block_file(&(vps->temp_file));

		void* free_space_mapper_page = NULL;
		uint64_t free_space_mapper_page_id;
		uint64_t page_id;

		while(new_active_page_count > 0)
		{
			page_id = new_active_page_count - 1;

			// we can directly truncate a trailing free space mapper page
			if(is_free_space_mapper_page(page_id, &(vps->stats)))
			{
				new_active_page_count--;
				continue;
			}

			// grab the free space mapper page and page_id, if not already mmaped
			if(free_space_mapper_page == NULL)
			{
				free_space_mapper_page_id = get_is_valid_bit_page_id_for_page(page_id, &(vps->stats));
				free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);
			}
			else
			{
				// we need to meddle only if the free_space_mapper_page that is mapped is not the right one
				if(free_space_mapper_page_id != get_is_valid_bit_page_id_for_page(page_id, &(vps->stats)))
				{
					// unmap the old one and map the new one
					release_page(&(vps->pool), free_space_mapper_page);

					free_space_mapper_page_id = get_is_valid_bit_page_id_for_page(page_id, &(vps->stats));
					free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);
				}
			}

			// if the corresponding allocation bit is set, then break out
			{
				uint64_t free_space_mapper_bit_pos = get_is_valid_bit_position_for_page(page_id, &(vps->stats));
				if(get_bit(free_space_mapper_page, free_space_mapper_bit_pos))
					break;
			}

			// page at page_id is free, to we can discard it
			new_active_page_count--;
			continue;
		}

		// unmap the free_space_mapper mapper page if any is mmaped
		if(free_space_mapper_page)
			release_page(&(vps->pool), free_space_mapper_page);

		// truncate only if there is more than 20% of unused space at the end of the database file
		if(vps->active_page_count - new_active_page_count > (vps->active_page_count * 0.2))
		{
			// before you shorten the file make sure that all free pages (which for sure are unreferenced) are unmaped
			discard_all_unreferenced_frame_descs(&(vps->pool));

			vps->active_page_count = new_active_page_count;
			if(!truncate_block_file(&(vps->temp_file), (block_count_per_page * new_active_page_count)))
			{
				printf("ISSUEv :: unable to truncate temp file\n");
				exit(-1);
			}
		}
	}
}

void* truncator(void* vps_v_p)
{
	volatile_page_store* vps = vps_v_p;

	pthread_mutex_lock(&(vps->global_lock));
	vps->is_truncator_running = 1;
	pthread_mutex_unlock(&(vps->global_lock));

	while(1)
	{
		pthread_mutex_lock(&(vps->global_lock));

		while(!vps->truncator_shutdown_called)
		{
			uint64_t period_in_microseconds = vps->truncator_period_in_microseconds;
			if(pthread_cond_timedwait_for_microseconds(&(vps->wait_for_truncator_period), &(vps->global_lock), &period_in_microseconds))
				break;
		}

		if(vps->truncator_shutdown_called)
		{
			pthread_mutex_unlock(&(vps->global_lock));
			break;
		}

		// perform truncation
		perform_truncation(vps);

		pthread_mutex_unlock(&(vps->global_lock));
	}

	pthread_mutex_lock(&(vps->global_lock));
	vps->is_truncator_running = 0;
	pthread_cond_broadcast(&(vps->wait_for_truncator_to_stop));
	pthread_mutex_unlock(&(vps->global_lock));

	return NULL;
}