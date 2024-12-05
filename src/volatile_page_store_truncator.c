#include<volatile_page_store_truncator.h>

#include<block_io_mmap_wrapper.h>

#include<system_page_header_util.h>

#include<bitmap.h>

#include<stdio.h>
#include<errno.h>
#include<stdlib.h>
#include<pthread.h>

#include<volatile_page_store.h>

void perform_truncation(volatile_page_store* vps)
{
	{
		// presering the free_space_mapper_page
		uint64_t free_space_mapper_page_id;
		void* free_space_mapper_page = NULL;

		// free all from free_pages_list
		while(vps->free_pages_list_head_page_id != vps->user_stats.NULL_PAGE_ID)
		{
			// grab the head's page_id and page
			uint64_t page_id = vps->free_pages_list_head_page_id;
			void* page = block_io_get_page(vps, page_id);

			// update free_pages_list_head_page_id, effectively popping head
			vps->free_pages_list_head_page_id = get_page_id_for_page(page, &(vps->stats));

			// write page_id on the page, so we do not have to remember this mapping
			set_page_id_for_page(page, page_id, &(vps->stats));

			// return page back to OS
			block_io_return_page(vps, page);

			// grab the free space mapper page and page_id, if not already mmaped
			if(free_space_mapper_page == NULL)
			{
				free_space_mapper_page_id = get_is_valid_bit_page_id_for_page(page_id, &(vps->stats));
				free_space_mapper_page = block_io_get_page(vps, free_space_mapper_page_id);
			}
			else
			{
				// we need to meddle only if the free_space_mapper_page that is mapped is not the right one
				if(free_space_mapper_page_id != get_is_valid_bit_page_id_for_page(page_id, &(vps->stats)))
				{
					// unmap the old one and map the new one
					block_io_return_page(vps, free_space_mapper_page);

					free_space_mapper_page_id = get_is_valid_bit_page_id_for_page(page_id, &(vps->stats));
					free_space_mapper_page = block_io_get_page(vps, free_space_mapper_page_id);
				}
			}

			// mark the page_id as free in free space map
			{
				uint64_t free_space_mapper_bit_pos = get_is_valid_bit_position_for_page(page_id, &(vps->stats));
				void* free_space_mapper_page_contents = get_page_contents_for_page(free_space_mapper_page, free_space_mapper_page_id, &(vps->stats));
				if(get_bit(free_space_mapper_page_contents, free_space_mapper_bit_pos) != 1) // this may never happen, we are just ensuring that the page being freed is indeed allocated
				{
					printf("ISSUEv :: bug in page being freed from free pages list, before truncation, page in free pages list is not marked allocated\n");
					exit(-1);
				}
				reset_bit(free_space_mapper_page_contents, free_space_mapper_bit_pos);
			}
		}

		// if any free space mapper page is mmaped release it
		if(free_space_mapper_page != NULL)
			block_io_return_page(vps, free_space_mapper_page);
	}
}

void* truncator(void* vps_v_p)
{
	volatile_page_store* vps = vps_v_p;

	pthread_mutex_lock(&(vps->manager_lock));
	vps->is_truncator_running = 1;
	pthread_mutex_unlock(&(vps->manager_lock));

	while(1)
	{
		pthread_mutex_lock(&(vps->manager_lock));

		while(!vps->truncator_shutdown_called)
		{
			struct timespec now;
			clock_gettime(CLOCK_REALTIME, &now);
			struct timespec diff = {.tv_sec = (vps->truncator_period_in_microseconds / 1000000LL), .tv_nsec = (vps->truncator_period_in_microseconds % 1000000LL) * 1000LL};
			struct timespec stop_at = {.tv_sec = now.tv_sec + diff.tv_sec, .tv_nsec = now.tv_nsec + diff.tv_nsec};
			stop_at.tv_sec += stop_at.tv_nsec / 1000000000LL;
			stop_at.tv_nsec = stop_at.tv_nsec % 1000000000LL;
			if(ETIMEDOUT == pthread_cond_timedwait(&(vps->wait_for_truncator_period), &(vps->manager_lock), &stop_at))
				break;
		}

		if(vps->truncator_shutdown_called)
		{
			pthread_mutex_unlock(&(vps->manager_lock));
			break;
		}

		// perform truncation
		perform_truncation(vps);

		pthread_mutex_unlock(&(vps->manager_lock));
	}

	pthread_mutex_lock(&(vps->manager_lock));
	vps->is_truncator_running = 0;
	pthread_cond_broadcast(&(vps->wait_for_truncator_to_stop));
	pthread_mutex_unlock(&(vps->manager_lock));

	return NULL;
}