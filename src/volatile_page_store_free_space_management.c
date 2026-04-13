#include<volatilepagestore/volatile_page_store_free_space_management.h>

#include<volatilepagestore/mmaped_file_pool.h>

#include<volatilepagestore/system_page_header_util.h>

#include<cutlery/bitmap.h>

#include<stdlib.h>
#include<stdio.h>

typedef struct free_list_vps_node free_list_vps_node;
struct free_list_vps_node
{
	uint64_t prev_page_id;
	uint64_t next_page_id;
};

void insert_in_free_pages_list_vps(volatile_page_store* vps, uint64_t page_id, void* page)
{
	// initialize the node on the page
	free_list_vps_node* page_node = page;
	page_node->prev_page_id = vps->user_stats.NULL_PAGE_ID;
	page_node->next_page_id = vps->user_stats.NULL_PAGE_ID;

	// insert it at head
	if(vps->free_pages_list_head_page_id != vps->user_stats.NULL_PAGE_ID) // if the head_page exists, make it's previous point to this new page
	{
		free_list_vps_node* head_page_node = acquire_page(&(vps->pool), vps->free_pages_list_head_page_id);
		head_page_node->prev_page_id = page_id;
		release_page(&(vps->pool), head_page_node);
	}
	page_node->next_page_id = vps->free_pages_list_head_page_id;

	vps->free_pages_list_head_page_id = page_id;

	release_page(&(vps->pool), page_node);
}

void* remove_from_free_pages_list_vps(volatile_page_store* vps, uint64_t page_id)
{
	free_list_vps_node* page_node = acquire_page(&(vps->pool), page_id);

	if(page_node->prev_page_id != vps->user_stats.NULL_PAGE_ID) // if a prev_page exists, re position its next
	{
		free_list_vps_node* prev_page_node = acquire_page(&(vps->pool), page_node->prev_page_id);
		prev_page_node->next_page_id = page_node->next_page_id;
		release_page(&(vps->pool), prev_page_node);
	}

	if(page_node->next_page_id != vps->user_stats.NULL_PAGE_ID) // if a next_page exists, re position its next
	{
		free_list_vps_node* next_page_node = acquire_page(&(vps->pool), page_node->next_page_id);
		next_page_node->prev_page_id = page_node->prev_page_id;
		release_page(&(vps->pool), next_page_node);
	}

	// redundant but we will initialize its next and prev page_ids
	page_node->prev_page_id = vps->user_stats.NULL_PAGE_ID;
	page_node->next_page_id = vps->user_stats.NULL_PAGE_ID;

	return page_node;
}

void mark_free_in_free_space_bitmap_page_vps(volatile_page_store* vps, uint64_t page_id)
{
	uint64_t free_space_mapper_page_id = get_is_valid_bit_page_id_for_page_vps(page_id, &(vps->stats));
	void* free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);

	uint64_t free_space_mapper_bit_pos = get_is_valid_bit_position_for_page_vps(page_id, &(vps->stats));
	if(get_bit(free_space_mapper_page, free_space_mapper_bit_pos) == 0)
	{
		printf("ISSUEv :: bug in page allocation or new page initialization, page attempting to be freed is already marked free\n");
		exit(-1);
	}
	reset_bit(free_space_mapper_page, free_space_mapper_bit_pos);

	release_page(&(vps->pool), free_space_mapper_page);
}

void mark_allocated_in_free_space_bitmap_page_vps(volatile_page_store* vps, uint64_t page_id)
{
	uint64_t free_space_mapper_page_id = get_is_valid_bit_page_id_for_page_vps(page_id, &(vps->stats));
	void* free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);

	uint64_t free_space_mapper_bit_pos = get_is_valid_bit_position_for_page_vps(page_id, &(vps->stats));
	if(get_bit(free_space_mapper_page, free_space_mapper_bit_pos) != 0)
	{
		printf("ISSUEv :: bug in page allocation or new page initialization, page attempting to be allocated is not marked free\n");
		exit(-1);
	}
	set_bit(free_space_mapper_page, free_space_mapper_bit_pos);

	release_page(&(vps->pool), free_space_mapper_page);
}

int is_free_page_vps(volatile_page_store* vps, uint64_t page_id)
{
	uint64_t free_space_mapper_page_id = get_is_valid_bit_page_id_for_page_vps(page_id, &(vps->stats));
	void* free_space_mapper_page = acquire_page(&(vps->pool), free_space_mapper_page_id);

	uint64_t free_space_mapper_bit_pos = get_is_valid_bit_position_for_page_vps(page_id, &(vps->stats));

	// it is free, if this bit is 0
	int is_free = (get_bit(free_space_mapper_page, free_space_mapper_bit_pos) == 0);

	release_page(&(vps->pool), free_space_mapper_page);

	return is_free;
}