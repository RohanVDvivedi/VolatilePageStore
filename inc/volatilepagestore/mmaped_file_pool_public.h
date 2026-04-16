#ifndef MMAPED_FILE_POOL_PUBLIC_H
#define MMAPED_FILE_POOL_PUBLIC_H

#include<blockio/block_io.h>

#include<pthread.h>

#include<stdint.h>

#include<cutlery/bst.h>
#include<cutlery/linkedlist.h>

// these many number of pages of the file will be mmaped and unmaped
// this sized block will be ftruncated to append to the file
#define MMAP_GROUP_SIZE 10000

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

	// each frame in this mmaped_file_pool is (MMAP_GROUP_SIZE * page_size) bytes big
	// and contains MMAP_GROUP_SIZE pages

	// bst => first_page_id (uint64_t) -> frame descriptor
	bst first_page_id_to_frame_desc;

	// bst => frame (void*) -> frame descriptor
	bst frame_ptr_to_frame_desc;

	// linkedlist of all frame_descs that are unreferenced, i.e. reference_counter = 0
	linkedlist unreferenced_frame_descs_lru_lists;
};

#endif