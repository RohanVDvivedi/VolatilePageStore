#include<mmaped_file_pool.h>

#include<frame_desc.h>

int initialize_mmaped_file_pool(mmaped_file_pool* mfp, pthread_mutex_t* external_lock, blockfile* file, uint64_t page_size, uint64_t hashmaps_bucket_count)
{
	if(hashmaps_bucket_count == 0)
		hashmaps_bucket_count = 128;

	if(page_size % sysconf(_SC_PAGESIZE) != 0) // page_size must be multiple of OS page size
		return 0;

	if(page_size % get_block_size_for_block_file(file) != 0) // page_size must also be multiple of block size of the disk
		return 0;

	mfp->has_internal_lock = (external_lock == NULL);
	if(mfp->has_internal_lock)
		pthread_mutex_init(&(mfp->internal_lock), NULL);
	else
		mfp->external_lock = external_lock;

	mfp->page_size = page_size;

	if(!initialize_hashmap(&(mfp->page_id_to_frame_desc), ELEMENTS_AS_RED_BLACK_BST, hashmaps_bucket_count, &simple_hasher(hash_frame_desc_by_page_id), &simple_comparator(compare_frame_desc_by_page_id), offsetof(frame_desc, embed_node_page_id_to_frame_desc)))
	{
		if(mfp->has_internal_lock)
			pthread_mutex_destroy(&(mfp->internal_lock));
		return 0;
	}

	if(!initialize_hashmap(&(mfp->frame_ptr_to_frame_desc), ELEMENTS_AS_RED_BLACK_BST, hashmaps_bucket_count, &simple_hasher(hash_frame_desc_by_frame_ptr), &simple_comparator(compare_frame_desc_by_frame_ptr), offsetof(frame_desc, embed_node_frame_ptr_to_frame_desc)))
	{
		deinitialize_hashmap(&(mfp->page_id_to_frame_desc));
		if(mfp->has_internal_lock)
			pthread_mutex_destroy(&(mfp->internal_lock));
		return 0;
	}

	initialize_linkedlist(&(mfp->unreferenced_frame_descs_lru_lists), offsetof(frame_desc, embed_node_unreferenced_lru_list));
}

void* acquire_page(mmaped_file_pool* mfp, uint64_t page_id);

int release_page(mmaped_file_pool* mfp, void* page);

uint64_t get_page_id_for_frame(mmaped_file_pool* mfp, const void* frame);

void discard_all_unreferenced_frame_descs(mmaped_file_pool* mfp);

void deinitialize_mmaped_file_pool(mmaped_file_pool* mfp);