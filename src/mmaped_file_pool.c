#include<blockio/block_io.h>
#include<volatilepagestore/mmaped_file_pool.h>

#include<volatilepagestore/frame_desc.h>

#include<sys/mman.h>
#include<stdio.h>
#include<stdlib.h>

int initialize_mmaped_file_pool(mmaped_file_pool* mfp, pthread_mutex_t* external_lock, block_file* file, uint64_t page_size)
{
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
	mfp->file = file;

	initialize_bst(&(mfp->first_page_id_to_frame_desc), RED_BLACK_TREE, &simple_comparator(compare_frame_desc_by_first_page_id), offsetof(frame_desc, embed_node_first_page_id_to_frame_desc));

	initialize_bst(&(mfp->frame_ptr_to_frame_desc), RED_BLACK_TREE, &simple_comparator(compare_frame_desc_by_frame_ptr), offsetof(frame_desc, embed_node_frame_ptr_to_frame_desc));

	initialize_linkedlist(&(mfp->unreferenced_frame_descs_lru_lists), offsetof(frame_desc, embed_node_unreferenced_lru_list));

	return 1;
}

static pthread_mutex_t* get_mmaped_file_pool_lock(mmaped_file_pool* mfp)
{
	if(mfp->has_internal_lock)
		return &(mfp->internal_lock);
	return mfp->external_lock;
}

static int insert_frame_desc(mmaped_file_pool* mfp, frame_desc* fd)
{
	return insert_in_bst(&(mfp->first_page_id_to_frame_desc), fd) && insert_in_bst(&(mfp->frame_ptr_to_frame_desc), fd);
}

static int remove_frame_desc(mmaped_file_pool* mfp, frame_desc* fd)
{
	return remove_from_bst(&(mfp->first_page_id_to_frame_desc), fd) && remove_from_bst(&(mfp->frame_ptr_to_frame_desc), fd);
}

// for the below 2 functions, we know that the frame_descriptor_mapping struct is at an offset 0 in frame_desc, with attribute name map,
// hence we can simply pass a stack allocated reference to this smaller struct to find the request frame_desc from the corresponding map
// this is an optimization allowing us to use lesser instantaneous stack space, since frame_desc is a huge struct

// For instance on my 64 bit x86_64 machine sizeof(frame_desc) yields 320 bytes, while a frame_desc_mapping yields just 16 bytes in size
// hence a major improvement on stack space usage, (also no need to initialize all of the frame_desc struct to 0)

static frame_desc* find_frame_desc_by_page_id(mmaped_file_pool* mfp, uint64_t page_id)
{
	frame_desc* fd = (frame_desc*) find_preceding_or_equals_in_bst(&(mfp->first_page_id_to_frame_desc), &((const frame_desc_mapping){.first_page_id = page_id}));
	if(fd == NULL)
		return NULL;

	// make sure that this page_id falls within the desired range
	if(fd->map.first_page_id <= page_id && page_id < (fd->map.first_page_id + MMAP_GROUP_SIZE))
		return fd;

	return NULL;
}

static frame_desc* find_frame_desc_by_ptr(mmaped_file_pool* mfp, void* ptr)
{
	frame_desc* fd = (frame_desc*) find_preceding_or_equals_in_bst(&(mfp->frame_ptr_to_frame_desc), &((const frame_desc_mapping){.frame = ptr}));
	if(fd == NULL)
		return NULL;

	// make sure that this pointer falls within the region
	if(memory_contains(fd->map.frame, MMAP_GROUP_SIZE * mfp->page_size, ptr))
		return fd;

	return NULL;
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
		if(munmap(fd->map.frame, mfp->page_size))
		{
			printf("ISSUEv :: munmap failed\n");
			exit(-1);
		}

		free(fd);
	}
}

void* acquire_page(mmaped_file_pool* mfp, uint64_t page_id)
{
	void* page_ptr = NULL;

	if(mfp->has_internal_lock)
		pthread_mutex_lock(get_mmaped_file_pool_lock(mfp));

	frame_desc* fd = find_frame_desc_by_page_id(mfp, page_id);
	if(fd == NULL) // need to create a new frame_desc for page_id
	{
		// mmap the page frame at its first_page_id
		uint64_t first_page_id = UINT_ALIGN_DOWN(page_id, MMAP_GROUP_SIZE);
		void* frame = mmap64(NULL, MMAP_GROUP_SIZE * mfp->page_size, PROT_READ | PROT_WRITE, MAP_SHARED, mfp->file->file_descriptor, first_page_id * mfp->page_size);
		if(frame == MAP_FAILED) // crash if mmap crashes
		{
			// MAP_FAILED possibly due to out of memory

			// relieve some memory pressure
			discard_all_unreferenced_frame_descs_UNSAFE(mfp);

			// and try again
			frame = mmap64(NULL, MMAP_GROUP_SIZE * mfp->page_size, PROT_READ | PROT_WRITE, MAP_SHARED, mfp->file->file_descriptor, first_page_id * mfp->page_size);

			// still failure, means panic
			if(frame == MAP_FAILED)
			{
				printf("ISSUEv :: could not mmap, reason : out of memory or page_id out of bounds\n");
				exit(-1);
			}
		}

		// malloc the fd
		fd = malloc(sizeof(frame_desc));
		if(fd == NULL)
		{
			printf("ISSUEv :: could not allocate frame_desc\n");
			exit(-1);
		}
		initialize_empty_frame_desc(fd);

		fd->map.first_page_id = first_page_id;
		fd->map.frame = frame;

		insert_frame_desc(mfp, fd);

		// we never kept it in unreferenced_lru_list
		fd->reference_counter = 1;

		goto EXIT;
	}

	fd->reference_counter++;

	// we incremented its reference_counter, so now it can not be kept in unreferenced list
	remove_from_linkedlist(&(mfp->unreferenced_frame_descs_lru_lists), fd);

	page_ptr = fd->map.frame + ((page_id - fd->map.first_page_id) * mfp->page_size);

	EXIT:;
	if(mfp->has_internal_lock)
		pthread_mutex_unlock(get_mmaped_file_pool_lock(mfp));

	return page_ptr;
}

int release_page(mmaped_file_pool* mfp, void* page_ptr)
{
	int res;

	if(mfp->has_internal_lock)
		pthread_mutex_lock(get_mmaped_file_pool_lock(mfp));

	frame_desc* fd = find_frame_desc_by_ptr(mfp, page_ptr);
	if(fd == NULL)
	{
		res = 0;
		goto EXIT;
	}

	// frame to be released was never acquired
	if(fd->reference_counter == 0)
	{
		printf("ISSUEv :: frame being relesed was never acquired\n");
		exit(-1);
	}

	fd->reference_counter--;
	if(fd->reference_counter == 0)
		insert_tail_in_linkedlist(&(mfp->unreferenced_frame_descs_lru_lists), fd);

	EXIT:;
	if(mfp->has_internal_lock)
		pthread_mutex_unlock(get_mmaped_file_pool_lock(mfp));

	return res;
}

uint64_t get_page_id_for_frame(mmaped_file_pool* mfp, const void* page_ptr)
{
	uint64_t page_id;

	if(mfp->has_internal_lock)
		pthread_mutex_lock(get_mmaped_file_pool_lock(mfp));

	frame_desc* fd = find_frame_desc_by_ptr(mfp, (void*)page_ptr);
	if(fd == NULL)
	{
		page_id = UINT64_MAX;
		goto EXIT;
	}

	page_id = fd->map.first_page_id + ((page_ptr - fd->map.frame) / mfp->page_size);

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

	//EXIT:;
	if(mfp->has_internal_lock)
		pthread_mutex_unlock(get_mmaped_file_pool_lock(mfp));
}

static void on_remove_all_from_first_page_id_to_frame_desc_bst_delete_frame_from_mmaped_file_pool(void* _mfp, const void* _fd)
{
	mmaped_file_pool* mfp = _mfp;
	frame_desc* fd = (frame_desc*) _fd;
	// this fd is already being removed from mfp->first_page_id_to_frame_desc
	// so we only need to remove it from mfp->frame_ptr_to_frame_desc
	remove_from_bst(&(mfp->frame_ptr_to_frame_desc), fd);
	munmap(fd->map.frame, mfp->page_size);
	free(fd);
}

void deinitialize_mmaped_file_pool(mmaped_file_pool* mfp)
{
	discard_all_unreferenced_frame_descs_UNSAFE(mfp);

	remove_all_from_bst(&(mfp->first_page_id_to_frame_desc), &((notifier_interface){NULL, on_remove_all_from_first_page_id_to_frame_desc_bst_delete_frame_from_mmaped_file_pool}));

	if(mfp->has_internal_lock)
		pthread_mutex_destroy(&(mfp->internal_lock));
}