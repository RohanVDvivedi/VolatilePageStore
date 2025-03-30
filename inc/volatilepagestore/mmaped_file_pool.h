#ifndef MMAPED_FILE_POOL_H
#define MMAPED_FILE_POOL_H

#include<volatilepagestore/mmaped_file_pool_public.h>

int initialize_mmaped_file_pool(mmaped_file_pool* mfp, pthread_mutex_t* external_lock, block_file* file, uint64_t page_size, uint64_t hashmaps_bucket_count);

void* acquire_page(mmaped_file_pool* mfp, uint64_t page_id);

int release_page(mmaped_file_pool* mfp, void* frame);

// get page_id for only an acquired page
uint64_t get_page_id_for_frame(mmaped_file_pool* mfp, const void* frame);

void discard_all_unreferenced_frame_descs(mmaped_file_pool* mfp);

void deinitialize_mmaped_file_pool(mmaped_file_pool* mfp);

#endif