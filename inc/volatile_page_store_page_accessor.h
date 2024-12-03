#ifndef VOLATILE_PAGE_STORE_PAGE_ACCESSOR_H
#define VOLATILE_PAGE_STORE_PAGE_ACCESSOR_H

#include<volatile_page_store.h>

void* get_new_page_for_vps(uint64_t* page_id);

void* acquire_page_for_vps(uint64_t page_id);

void release_page_for_vs(void* page, int free_page);

void free_page_for_vps(uint64_t page_id);

#endif