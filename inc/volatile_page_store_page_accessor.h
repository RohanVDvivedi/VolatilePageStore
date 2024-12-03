#ifndef VOLATILE_PAGE_STORE_PAGE_ACCESSOR_H
#define VOLATILE_PAGE_STORE_PAGE_ACCESSOR_H

#include<volatile_page_store.h>

void* get_new_page_for_vps(volatile_page_store* vps, uint64_t* page_id);

void* acquire_page_for_vps(volatile_page_store* vps, uint64_t page_id);

void release_page_for_vs(volatile_page_store* vps, void* page_contents, int free_page);

void free_page_for_vps(volatile_page_store* vps, uint64_t page_id);

#endif