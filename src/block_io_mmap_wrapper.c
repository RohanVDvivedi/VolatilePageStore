#include<block_io.h>
#include<block_io_mmap_wrapper.h>

#include<system_page_header_util.h>

#include<sys/mman.h>

#include<stdlib.h>
#include<stdio.h>

void* block_io_get_page(volatile_page_store* vps, uint64_t page_id)
{
	if(page_id >= vps->user_stats.max_page_count)
	{
		printf("ISSUEv :: overflow in page_id\n");
		exit(-1);
	}

	void* page = mmap(NULL, vps->stats.page_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, vps->temp_file.file_descriptor, page_id * vps->stats.page_size);
	if(page == MAP_FAILED)
	{
		printf("ISSUEv :: mmap failed\n");
		exit(-1);
	}

	if(!validate_page_checksum(page, &(vps->stats)))
	{
		printf("ISSUEv :: checksum vaildation failed\n");
		exit(-1);
	}

	return page;
}

void block_io_return_page(volatile_page_store* vps, void* page)
{
	recalculate_page_checksum(page, &(vps->stats));
	if(munmap(page, vps->stats.page_size))
	{
		printf("ISSUEv :: munmap failed\n");
		exit(-1);
	}
}

void expand_file_with_zero_page(volatile_page_store* vps, uint64_t* page_id)
{
}