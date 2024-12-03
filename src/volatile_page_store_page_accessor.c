#include<volatile_page_store_page_accessor.h>

#include<block_io_mmap_wrapper.h>

void* get_new_page_for_vps(volatile_page_store* vps, uint64_t* page_id)
{
	// TODO
}

void* acquire_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// TODO
}

void release_page_for_vs(volatile_page_store* vps, void* page, int free_page)
{
	// TODO
}

void free_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// TODO
}