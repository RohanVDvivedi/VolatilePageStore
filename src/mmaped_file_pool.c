#include<mmaped_file_pool.h>

#include<frame_desc.h>

int initialize_mmaped_file_pool(mmaped_file_pool* mfp, pthread_mutex_t* external_lock, blockfile* file, uint64_t page_size, uint64_t hashmaps_bucket_count);

void* acquire_page(mmaped_file_pool* mfp, uint64_t page_id);

int release_page(mmaped_file_pool* mfp, void* page);

uint64_t get_page_id_for_frame(mmaped_file_pool* mfp, const void* frame);

void discard_all_unreferenced_frame_descs(mmaped_file_pool* mfp);

void deinitialize_mmaped_file_pool(mmaped_file_pool* mfp);