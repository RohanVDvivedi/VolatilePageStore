#ifndef MMAPED_FILE_POOL_PUBLIC_H
#define MMAPED_FILE_POOL_PUBLIC_H

#include<block_io.h>

#include<pthread.h>
#include<hashmap.h>
#include<linkedlist.h>

#include<stdint.h>

typedef struct mmaped_file_pool mmaped_file_pool;
struct mmaped_file_pool
{
	block_file* file;

	int has_internal_lock : 1;
	union
	{
		pthread_mutex_t* external_lock;
		pthread_mutex_t  internal_lock;
	};

	uint64_t page_size;

	// hashtable => page_id (uint64_t) -> frame descriptor
	hashmap page_id_to_frame_desc;

	// hashtable => frame (void*) -> frame descriptor
	hashmap frame_ptr_to_frame_desc;

	// linkedlist of all frame_descs that are unreferenced, i.e. reference_counter = 0
	linkedlist unreferenced_frame_descs_lru_lists;
};

#endif