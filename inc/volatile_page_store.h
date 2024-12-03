#ifndef VOLATILE_PAGE_STORE_H
#define VOLATILE_PAGE_STORE_H

#include<block_io.h>

#include<volatile_page_store_stats.h>

#include<pthread.h>

typedef struct volatile_page_store volatile_page_store;
struct volatile_page_store
{
	// you need to acquire manager_lock,
	/*
		if
			1. to allocate/deallocate pages (even if using free_page_list_head_page_id)
			2. truncate expand the temp_file
			3. and during truncation job, which frees all pages from free_page_list_head_page_id, and then discards all the trailing free pages from the file
	*/
	pthread_mutex_t manager_lock;

	block_file temp_file;

	// free list of data pages, linked by their page_id slot in the system page header
	// all the pages present here in this list will still already be marked as allocated in their corresponding free space mapper pages,
	// but inserting a new one here or removing one needs to be done with manager lock held
	// this list just serves as a quick way to allocate and deallocate
	// it must be accessed with manager_lock
	uint64_t free_page_list_head_page_id;

	uint64_t active_page_count;

	// stats for internal use
	volatile_page_store_stats stats;

	// stats to be used by user
	volatile_page_store_user_stats user_stats;

	// below attributes are use for truncator thread only

	uint64_t truncator_period_in_microseconds;
};

int initialize_volatile_page_store(volatile_page_store* vps, const char* directory, uint32_t page_size, uint32_t page_id_width, uint64_t truncator_period_in_microseconds);

#include<volatile_page_store_page_accessor.h>

#endif