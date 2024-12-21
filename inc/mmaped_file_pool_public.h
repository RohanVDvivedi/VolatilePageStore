#ifndef MMAPED_FILE_POOL_PUBLIC_H
#define MMAPED_FILE_POOL_PUBLIC_H

#include<pthread.h>
#include<hashmap.h>
#include<linkedlist.h>

struct mmaped_file_pool
{
	blockfile* file;

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