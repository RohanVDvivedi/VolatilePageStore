#include<volatile_page_store_page_accessor.h>

#include<block_io_mmap_wrapper.h>

#include<system_page_header_util.h>

#include<stdlib.h>
#include<stdio.h>

void* get_new_page_for_vps(volatile_page_store* vps, uint64_t* page_id)
{
	// TODO
}

void* acquire_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// page_id must not be a free space mapper page
	if(is_free_space_mapper_page(page_id, &(vps->stats)))
	{
		printf("ISSUEv :: user accessing free space mapper page\n");
		exit(-1);
	}

	// get the page
	void* page = block_io_get_page(vps, page_id);

	// write page_id on the page, so we do not have to remember this mapping
	set_page_id_for_page(page, page_id, &(vps->stats));

	// return the page contents for this page
	return get_page_contents_for_page(page, page_id, &(vps->stats));
}

void release_page_for_vs(volatile_page_store* vps, void* page_contents, int free_page)
{
	// get page from the page contents
	void* page = page_contents - get_system_header_size_for_data_pages(&(vps->stats));

	if(!free_page)
		block_io_return_page(vps, page);
	else
	{
		// grab page id of the page
		uint64_t page_id = get_page_id_for_page(page, &(vps->stats));

		// put this page in the head of the free_pages_list
		{
			pthread_mutex_lock(&(vps->manager_lock));

				set_page_id_for_page(page, vps->free_page_list_head_page_id, &(vps->stats));

				vps->free_page_list_head_page_id = page_id;

				block_io_return_page(vps, page);

			pthread_mutex_unlock(&(vps->manager_lock));
		}
	}
}

void free_page_for_vps(volatile_page_store* vps, uint64_t page_id)
{
	// TODO
}