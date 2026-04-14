#include<volatilepagestore/volatile_page_store_truncator.h>

#include<volatilepagestore/system_page_header_util.h>

#include<cutlery/bitmap.h>

#include<stdio.h>
#include<errno.h>
#include<stdlib.h>
#include<pthread.h>

#include<posixutils/pthread_cond_utils.h>

#include<volatilepagestore/volatile_page_store.h>
#include<volatilepagestore/mmaped_file_pool.h>
#include<volatilepagestore/volatile_page_store_free_space_management.h>

static void perform_truncation(volatile_page_store* vps)
{
	if(vps->active_page_count > 0)
	{
		uint64_t new_active_page_count = vps->active_page_count;

		while(new_active_page_count > 0)
		{
			uint64_t page_id = new_active_page_count-1;

			// we can directly truncate a trailing free space mapper page
			if(is_free_space_mapper_page_vps(page_id, &(vps->stats)))
			{
				new_active_page_count--;
				continue;
			}

			// else if it is free, then also we truncate it
			if(is_free_page_vps(vps, page_id))
			{
				void* page = remove_from_free_pages_list_vps(vps, page_id);
				release_page(&(vps->pool), page);
				new_active_page_count--;
				continue;
			}

			break;
		}

		// truncate the file
		{
			// before you shorten the file make sure that all free pages (which for sure are unreferenced) are unmaped
			discard_all_unreferenced_frame_descs(&(vps->pool));

			uint64_t block_count_per_page = vps->stats.page_size / get_block_size_for_block_file(&(vps->temp_file));
			vps->active_page_count = new_active_page_count;
			if(!truncate_block_file(&(vps->temp_file), (block_count_per_page * new_active_page_count)))
			{
				printf("ISSUEv :: unable to truncate temp file\n");
				exit(-1);
			}
		}
	}
}

void truncator_function(void* vps_v_p)
{
	volatile_page_store* vps = vps_v_p;

	pthread_mutex_lock(&(vps->global_lock));

		// perform truncation
		perform_truncation(vps);

	pthread_mutex_unlock(&(vps->global_lock));
}