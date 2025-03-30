#include<volatilepagestore/system_page_header_util.h>

uint64_t is_valid_bits_count_on_free_space_mapper_page(const volatile_page_store_stats* stats)
{
	return ((uint64_t)(stats->page_size)) * UINT64_C(8);
}

#define PAGE_POS_MULTIPLIER(stats) (is_valid_bits_count_on_free_space_mapper_page(stats) + UINT64_C(1))

int is_free_space_mapper_page(uint64_t page_id, const volatile_page_store_stats* stats)
{
	return (page_id % PAGE_POS_MULTIPLIER(stats)) == 0;
}

uint64_t get_is_valid_bit_page_id_for_page(uint64_t page_id, const volatile_page_store_stats* stats)
{
	return (page_id / PAGE_POS_MULTIPLIER(stats)) * PAGE_POS_MULTIPLIER(stats);
}

uint64_t get_is_valid_bit_position_for_page(uint64_t page_id, const volatile_page_store_stats* stats)
{
	return (page_id % PAGE_POS_MULTIPLIER(stats)) - UINT64_C(1);
}