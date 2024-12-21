#include<volatile_page_store.h>

#include<mmaped_file_pool.h>
#include<volatile_page_store_truncator.h>

#include<stdio.h>

int initialize_volatile_page_store(volatile_page_store* vps, const char* directory, uint32_t page_size, uint32_t page_id_width, uint64_t truncator_period_in_microseconds)
{
	pthread_mutex_init(&(vps->global_lock), NULL);
	vps->active_page_count = 0;

	if(page_size % sysconf(_SC_PAGESIZE) != 0) // page_size must be multiple of OS page size
		return 0;

	if(page_id_width == 0 || page_id_width > 8) // page_id_width must be between 1 to 8 both inclusive
		return 0;

	if(!temp_block_file(&(vps->temp_file), directory, 0))
		return 0;

	if(page_size % get_block_size_for_block_file(&(vps->temp_file)) != 0) // page_size must also be multiple of block size of the disk
	{
		close_block_file(&(vps->temp_file));
		return 0;
	}

	vps->stats.page_size = page_size;
	vps->stats.page_id_width = page_id_width;

	vps->user_stats = get_volatile_page_store_user_stats(&(vps->stats), get_block_size_for_block_file(&(vps->temp_file)));

	vps->free_pages_list_head_page_id = vps->user_stats.NULL_PAGE_ID;

	if(!initialize_mmaped_file_pool(&(vps->pool), &(vps->global_lock), &(vps->temp_file), page_size, 1000))
	{
		close_block_file(&(vps->temp_file));
		return 0;
	}

	// initialize and start the truncator

	vps->truncator_period_in_microseconds = truncator_period_in_microseconds;
	pthread_cond_init(&(vps->wait_for_truncator_period), NULL);
	pthread_cond_init(&(vps->wait_for_truncator_to_stop), NULL);
	vps->is_truncator_running = 0;
	vps->truncator_shutdown_called = 0;

	// start truncator thread
	initialize_job(&(vps->truncator_job), truncator, vps, NULL, NULL);
	pthread_t thread_id;
	int truncator_job_start_error = execute_job_async(&(vps->truncator_job), &thread_id);
	if(truncator_job_start_error)
	{
		printf("ISSUEv :: could not start truncator job\n");
		exit(-1);
	}

	return 1;
}

void deinitialize_volatile_page_store(volatile_page_store* vps)
{
	pthread_mutex_lock(&(vps->manager_lock));

	vps->truncator_shutdown_called = 1;

	// wake up truncator and wait for it to stop
	pthread_cond_signal(&(vps->wait_for_truncator_period));
	while(vps->is_truncator_running)
		pthread_cond_wait(&(vps->wait_for_truncator_to_stop), &(vps->manager_lock));

	deinitialize_mmaped_file_pool(&(vps->pool));

	pthread_mutex_unlock(&(vps->manager_lock));

	pthread_mutex_destroy(&(vps->global_lock));
	pthread_cond_destroy(&(vps->wait_for_truncator_period));
	pthread_cond_destroy(&(vps->wait_for_truncator_to_stop));

	close_block_file(&(vps->temp_file));
}