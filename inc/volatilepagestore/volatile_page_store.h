#ifndef VOLATILE_PAGE_STORE_H
#define VOLATILE_PAGE_STORE_H

#include<blockio/block_io.h>

#include<volatilepagestore/mmaped_file_pool_public.h>
#include<volatilepagestore/volatile_page_store_stats.h>

#include<pthread.h>
#include<boompar/periodic_job.h>

typedef struct volatile_page_store volatile_page_store;
struct volatile_page_store
{
	// you need to acquire manager_lock, for any operation
	pthread_mutex_t global_lock;

	block_file temp_file;

	// free list of data pages, linked by their page_id slot in the system page header
	// all the pages present here in this list will still already be marked as allocated in their corresponding free space mapper pages,
	// but inserting a new one here or removing one needs to be done with manager lock held
	// this list just serves as a quick way to allocate and deallocate
	// it must be accessed with manager_lock
	uint64_t free_pages_list_head_page_id;

	uint64_t active_page_count;

	// stats for internal use
	volatile_page_store_stats stats;

	// stats to be used by user
	volatile_page_store_user_stats user_stats;

	// pool of mmaped pages, to avoid system call overhead and evictions of frequently used pages
	mmaped_file_pool pool;

	// periodic_job for truncator
	periodic_job* periodic_truncator_job;
};

int initialize_volatile_page_store(volatile_page_store* vps, const char* directory, uint32_t page_size, uint32_t page_id_width, uint64_t truncator_period_in_microseconds);

#include<volatilepagestore/volatile_page_store_page_accessor.h>

void deinitialize_volatile_page_store(volatile_page_store* vps);

int update_truncator_period_for_volatile_page_store(volatile_page_store* vps, uint64_t truncator_period_in_microseconds);

#endif