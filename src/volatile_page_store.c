#include<volatile_page_store.h>

int initialize_volatile_page_store(volatile_page_store* vps, const char* directory, uint32_t page_size, uint32_t page_id_width, uint64_t truncator_period_in_microseconds)
{
	pthread_mutex_init(&(vps->manager_lock), NULL);
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

	// initialize and start the truncator

	vps->truncator_period_in_microseconds = truncator_period_in_microseconds;

	return 1;
}