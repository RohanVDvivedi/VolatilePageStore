#ifndef SYSTEM_PAGE_HEADER_UTIL_H
#define SYSTEM_PAGE_HEADER_UTIL_H

#include<large_uints.h>

#include<volatile_page_store_stats.h>

/*
	system header consists of only 1 thing
	page_id  - as wide as page_id_width on all data pages
             - it is not present on the free space bitmap pages
*/

int is_free_space_mapper_page(uint64_t page_id, const volatile_page_store_stats* stats);

uint64_t is_valid_bits_count_on_free_space_mapper_page(const volatile_page_store_stats* stats);

// get page_id and bit_index for the is_valid bit of the page
// the result are invalid if the page itself is a free_space_mapper_page
uint64_t get_is_valid_bit_page_id_for_page(uint64_t page_id, const volatile_page_store_stats* stats);
uint64_t get_is_valid_bit_position_for_page(uint64_t page_id, const volatile_page_store_stats* stats);

// logically !is_free_space_mapper_page(), free space mapper page does not have page_id
int has_page_id_on_page(uint64_t page_id, const volatile_page_store_stats* stats);

// get/set page_id on to the page
// i.e. page_id is the id of this page, this allows us to not have a map in volatile page store
uint64_t get_page_id_for_page(const void* page, const volatile_page_store_stats* stats);
int set_page_id_for_page(void* page, uint64_t page_id, const volatile_page_store_stats* stats);

// if it is a free_space_mapper page, then it contains only 4 byte check_sum
// else it contains 4 byte checksum, and page_id
uint32_t get_system_header_size_for_page(uint64_t page_id, const volatile_page_store_stats* stats);
uint32_t get_page_content_size_for_page(uint64_t page_id, const volatile_page_store_stats* stats);

uint32_t get_system_header_size_for_data_pages(const volatile_page_store_stats* stats);

uint32_t get_page_content_size_for_data_pages(const volatile_page_store_stats* stats);
uint32_t get_page_content_size_for_free_space_mapper_pages(const volatile_page_store_stats* stats);

// adds system header size for the page to the page
void* get_page_contents_for_page(void* page, uint64_t page_id, const volatile_page_store_stats* stats);
void* get_page_for_page_contents(void* page_contents, uint64_t page_id, const volatile_page_store_stats* stats);

#endif