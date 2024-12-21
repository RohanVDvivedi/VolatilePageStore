#ifndef SYSTEM_PAGE_HEADER_UTIL_H
#define SYSTEM_PAGE_HEADER_UTIL_H

#include<large_uints.h>

#include<volatile_page_store_stats.h>

int is_free_space_mapper_page(uint64_t page_id, const volatile_page_store_stats* stats);

uint64_t is_valid_bits_count_on_free_space_mapper_page(const volatile_page_store_stats* stats);

// get page_id and bit_index for the is_valid bit of the page
// the result are invalid if the page itself is a free_space_mapper_page
uint64_t get_is_valid_bit_page_id_for_page(uint64_t page_id, const volatile_page_store_stats* stats);
uint64_t get_is_valid_bit_position_for_page(uint64_t page_id, const volatile_page_store_stats* stats);

#endif