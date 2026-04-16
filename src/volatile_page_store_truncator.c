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
	// make trailing active_pages that are free, into inactive_pages
	while(vps->active_page_count > 0)
	{
		uint64_t page_id = (vps->active_page_count-1);

		// we can directly truncate a trailing free space mapper page
		if(is_free_space_mapper_page_vps(page_id, &(vps->stats)))
		{
			vps->active_page_count--;
			continue;
		}

		// else if it is free, then also we truncate it
		if(is_free_page_vps(vps, page_id))
		{
			void* page = remove_from_free_pages_list_vps(vps, page_id);
			release_page(&(vps->pool), page);
			vps->active_page_count--;
			continue;
		}

		break;
	}

	// truncate the file
	{
		uint64_t new_total_page_count = UINT_ALIGN_UP(vps->active_page_count, MMAP_GROUP_SIZE);
		if(vps->total_page_count > new_total_page_count)
		{
			vps->total_page_count = new_total_page_count;

			// before you shorten the file make sure that all free pages (which for sure are unreferenced) are unmaped
			discard_all_unreferenced_frame_descs(&(vps->pool));

			if(!truncate_block_file(&(vps->temp_file), vps->total_page_count * (vps->stats.page_size / get_block_size_for_block_file(&(vps->temp_file)))))
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