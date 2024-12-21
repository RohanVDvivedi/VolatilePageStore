#include<block_io.h>
#include<mmaped_file_pool.h>

#include<frame_desc.h>

int initialize_mmaped_file_pool(mmaped_file_pool* mfp, pthread_mutex_t* external_lock, block_file* file, uint64_t page_size, uint64_t hashmaps_bucket_count)
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

static pthread_mutex_t* get_mmaped_file_pool_lock(mmaped_file_pool* mfp)
{
	if(mfp->has_internal_lock)
		return &(mfp->internal_lock);
	return mfp->external_lock;
}

static int insert_frame_desc(mmaped_file_pool* mfp, frame_desc* fd)
{
	return insert_in_hashmap(&(mfp->page_id_to_frame_desc), fd) && insert_in_hashmap(&(mfp->frame_ptr_to_frame_desc), fd);
}

static int remove_frame_desc(mmaped_file_pool* mfp, frame_desc* fd)
{
	return remove_from_hashmap(&(mfp->page_id_to_frame_desc), fd) && remove_from_hashmap(&(mfp->frame_ptr_to_frame_desc), fd);
}

// for the below 2 functions, we know that the frame_descriptor_mapping struct is at an offset 0 in frame_desc, with attribute name map,
// hence we can simply pass a stack allocated reference to this smaller struct to find the request frame_desc from the corresponding map
// this is an optimization allowing us to use lesser instantaneous stack space, since frame_desc is a huge struct

// For instance on my 64 bit x86_64 machine sizeof(frame_desc) yields 320 bytes, while a frame_desc_mapping yields just 16 bytes in size
// hence a major improvement on stack space usage, (also no need to initialize all of the frame_desc struct to 0)

static frame_desc* find_frame_desc_by_page_id(mmaped_file_pool* mfp, uint64_t page_id)
{
	return (frame_desc*) find_equals_in_hashmap(&(mfp->page_id_to_frame_desc), &((const frame_desc_mapping){.page_id = page_id}));
}

static frame_desc* find_frame_desc_by_frame_ptr(mmaped_file_pool* mfp, void* frame)
{
	return (frame_desc*) find_equals_in_hashmap(&(mfp->frame_ptr_to_frame_desc), &((const frame_desc_mapping){.frame = frame}));
}

static void discard_all_unreferenced_frame_descs_UNSAFE(mmaped_file_pool* mfp)
{
	while(!is_empty_linkedlist(&(mfp->unreferenced_frame_descs_lru_lists)))
	{
		// get head to process and remove it from lru list
		frame_desc* fd = (frame_desc*) get_head_of_linkedlist(&(mfp->unreferenced_frame_descs_lru_lists));
		remove_head_from_linkedlist(&(mfp->unreferenced_frame_descs_lru_lists));

		remove_frame_desc(mfp, fd);

		// munmap, if fails crash
		if(munmap(fd->frame, mfp->page_size))
		{
			printf("ISSUEv :: munmap failed\n");
			exit(-1);
		}

		free(fd);
	}
}

void* acquire_page(mmaped_file_pool* mfp, uint64_t page_id)
{
	if(mfp->has_internal_lock)
		pthread_mutex_lock(get_mmaped_file_pool_lock(mfp));

	// TODO

	EXIT:;
	if(mfp->has_internal_lock)
		pthread_mutex_unlock(get_mmaped_file_pool_lock(mfp));
}

int release_page(mmaped_file_pool* mfp, void* frame)
{
	int res;

	if(mfp->has_internal_lock)
		pthread_mutex_lock(get_mmaped_file_pool_lock(mfp));

	frame_desc* fd = find_frame_desc_by_frame_ptr(mfp, frame);
	if(fd == NULL)
	{
		res = 0;
		goto EXIT;
	}

	fd->reference_counter--;
	if(fd->reference_counter == 0)
		insert_tail_in_linkedlist(&(mfp->unreferenced_frame_descs_lru_lists), fd);

	EXIT:;
	if(mfp->has_internal_lock)
		pthread_mutex_unlock(get_mmaped_file_pool_lock(mfp));

	return res;
}

uint64_t get_page_id_for_frame(mmaped_file_pool* mfp, const void* frame)
{
	uint64_t page_id;

	if(mfp->has_internal_lock)
		pthread_mutex_lock(get_mmaped_file_pool_lock(mfp));

	frame_desc* fd = find_frame_desc_by_frame_ptr(mfp, frame);
	if(fd == NULL)
	{
		page_id = 0;
		goto EXIT;
	}

	page_id = fd->map.page_id;

	EXIT:;
	if(mfp->has_internal_lock)
		pthread_mutex_unlock(get_mmaped_file_pool_lock(mfp));

	return page_id;
}

void discard_all_unreferenced_frame_descs(mmaped_file_pool* mfp)
{
	if(mfp->has_internal_lock)
		pthread_mutex_lock(get_mmaped_file_pool_lock(mfp));

	discard_all_unreferenced_frame_descs_UNSAFE(mfp);

	EXIT:;
	if(mfp->has_internal_lock)
		pthread_mutex_unlock(get_mmaped_file_pool_lock(mfp));
}

void deinitialize_mmaped_file_pool(mmaped_file_pool* mfp);