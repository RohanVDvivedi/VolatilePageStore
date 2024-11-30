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
		1. to allocate/deallocate pages
		2. truncate expand the temp_file
	*/
	pthread_mutex_t manager_lock;

	block_file temp_file;

	uint64_t database_page_count;

	// stats for internal use
	volatile_page_store_stats stats;

	// stats to be used by user
	volatile_page_store_user_stats user_stats;
};

#endif