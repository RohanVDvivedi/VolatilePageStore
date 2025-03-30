#ifndef VOLATILE_PAGE_STORE_PAGE_ACCESSOR_H
#define VOLATILE_PAGE_STORE_PAGE_ACCESSOR_H

#include<volatilepagestore/volatile_page_store.h>

void* get_new_page_for_vps(volatile_page_store* vps, uint64_t* page_id);

// the below three functions do not check if the page/page_id provided is actually free or not OR whether it is in free_pages_list or not
// if a page is in free_pages_list it is still marked as allocated on its free space mapper page
// so there is no way of checking that the page is in free_page_list or is allocated
// on the other side, checking if the page is allocated in the free space mapper page requires 1 more read IO of the corresponding free space mapper page, so for efficiency reason we do not do it here

void* acquire_page_for_vps(volatile_page_store* vps, uint64_t page_id);

// freeing the page with the below functions, will just put it in to the free_pages_list at its head

void release_page_for_vps(volatile_page_store* vps, void* page, int free_page);

void free_page_for_vps(volatile_page_store* vps, uint64_t page_id);

#endif