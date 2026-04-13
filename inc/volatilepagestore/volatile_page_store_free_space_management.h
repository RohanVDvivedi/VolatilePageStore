#ifndef VOLATILE_PAGE_STORE_FREE_SPACE_MANAGEMENT_H
#define VOLATILE_PAGE_STORE_FREE_SPACE_MANAGEMENT_H

#include<volatilepagestore/volatile_page_store.h>

void insert_in_free_pages_list_vps(volatile_page_store* vps, uint64_t page_id, void* page);

void* remove_from_free_pages_list_vps(volatile_page_store* vps, uint64_t page_id);

void mark_free_in_free_space_bitmap_page_vps(volatile_page_store* vps, uint64_t page_id);

void mark_allocated_in_free_space_bitmap_page_vps(volatile_page_store* vps, uint64_t page_id);

int is_free_page_vps(volatile_page_store* vps, uint64_t page_id);

#endif