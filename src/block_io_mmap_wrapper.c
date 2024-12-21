#include<block_io.h>
#include<block_io_mmap_wrapper.h>

#include<sys/mman.h>

#include<stdlib.h>
#include<stdio.h>

void* block_io_get_page(volatile_page_store* vps, uint64_t page_id)
{
	// if the page_id is out of range crash
	if(page_id >= vps->user_stats.max_page_count)
	{
		printf("ISSUEv :: overflow in page_id\n");
		exit(-1);
	}

	// mmap the page, if fails crash
	void* page = mmap(NULL, vps->stats.page_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, vps->temp_file.file_descriptor, page_id * vps->stats.page_size);
	if(page == MAP_FAILED)
	{
		printf("ISSUEv :: mmap failed\n");
		exit(-1);
	}

	return page;
}

void block_io_return_page(volatile_page_store* vps, void* page)
{
	// munmap, if fails crash
	if(munmap(page, vps->stats.page_size))
	{
		printf("ISSUEv :: munmap failed\n");
		exit(-1);
	}
}

void* block_io_get_new_page(volatile_page_store* vps, uint64_t* page_id)
{
	// can not expand beyong max_page_count
	if(vps->active_page_count == vps->user_stats.max_page_count)
	{
		printf("ISSUEv :: out of available page ids for expansion\n");
		exit(-1);
	}

	// initialize new page id
	(*page_id) = ((vps->active_page_count)++);

	// expand the file
	{
		uint64_t block_count = (vps->active_page_count / get_block_size_for_block_file(&(vps->temp_file))) * (vps->stats.page_size);
		if(!truncate_block_file(&(vps->temp_file), block_count))
		{
			printf("ISSUEv :: could not expand the file\n");
			exit(-1);
		}
	}

	// mmap the file, recalculate checksum and them munmap it
	void* page = mmap(NULL, vps->stats.page_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, vps->temp_file.file_descriptor, (*page_id) * vps->stats.page_size);
	if(page == MAP_FAILED) // crash if mmap crashes
	{
		printf("ISSUEv :: mmap failed expanding the file\n");
		exit(-1);
	}

	return page;
}