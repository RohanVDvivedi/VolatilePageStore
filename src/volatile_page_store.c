#include<volatilepagestore/volatile_page_store.h>

#include<volatilepagestore/mmaped_file_pool.h>
#include<volatilepagestore/volatile_page_store_truncator.h>

#include<posixutils/pthread_cond_utils.h>

#include<stdio.h>

#define MINIMUM_TRUNCATOR_PERIOD (5000000) // 5 seconds

int initialize_volatile_page_store(volatile_page_store* vps, const char* directory, uint32_t page_size, uint32_t page_id_width, uint64_t truncator_period_in_microseconds)
{
	vps->active_page_count = 0;

	if(page_size % sysconf(_SC_PAGESIZE) != 0) // page_size must be multiple of OS page size
		return 0;

	if(page_id_width == 0 || page_id_width > 8) // page_id_width must be between 1 to 8 both inclusive
		return 0;

	if(truncator_period_in_microseconds == BLOCKING || truncator_period_in_microseconds == NON_BLOCKING)
		return 0;

	if(truncator_period_in_microseconds < MINIMUM_TRUNCATOR_PERIOD)
		return 0;

	if(!temp_block_file(&(vps->temp_file), directory, 0))
		return 0;

	pthread_mutex_init(&(vps->global_lock), NULL);

	if(page_size % get_block_size_for_block_file(&(vps->temp_file)) != 0) // page_size must also be multiple of block size of the disk
	{
		pthread_mutex_destroy(&(vps->global_lock));
		close_block_file(&(vps->temp_file));
		return 0;
	}

	vps->stats.page_size = page_size;
	vps->stats.page_id_width = page_id_width;

	vps->user_stats = get_volatile_page_store_user_stats(&(vps->stats), get_block_size_for_block_file(&(vps->temp_file)));

	vps->free_pages_list_head_page_id = vps->user_stats.NULL_PAGE_ID;

	if(!initialize_mmaped_file_pool(&(vps->pool), &(vps->global_lock), &(vps->temp_file), page_size, 1000))
	{
		pthread_mutex_destroy(&(vps->global_lock));
		close_block_file(&(vps->temp_file));
		return 0;
	}

	// initialize and start the truncator

	vps->periodic_truncator_job = new_periodic_job(truncator_function, vps, truncator_period_in_microseconds);
	if(vps->periodic_truncator_job == NULL)
	{
		deinitialize_mmaped_file_pool(&(vps->pool));
		pthread_mutex_destroy(&(vps->global_lock));
		close_block_file(&(vps->temp_file));
		return 0;
	}

	resume_periodic_job(vps->periodic_truncator_job);

	return 1;
}

void deinitialize_volatile_page_store(volatile_page_store* vps)
{
	// delete and shutdown periodic truncator job
	delete_periodic_job(vps->periodic_truncator_job);

	deinitialize_mmaped_file_pool(&(vps->pool));

	pthread_mutex_destroy(&(vps->global_lock));

	close_block_file(&(vps->temp_file));
}

int update_truncator_period_for_volatile_page_store(volatile_page_store* vps, uint64_t truncator_period_in_microseconds)
{
	if(truncator_period_in_microseconds < MINIMUM_TRUNCATOR_PERIOD)
		return 0;

	return update_period_for_periodic_job(vps->periodic_truncator_job, truncator_period_in_microseconds);
}