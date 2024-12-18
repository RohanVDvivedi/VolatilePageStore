#include<system_page_header_util.h>

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

int has_page_id_on_page(uint64_t page_id, const volatile_page_store_stats* stats)
{
	return !is_free_space_mapper_page(page_id, stats);
}

uint64_t get_page_id_for_page(const void* page, const volatile_page_store_stats* stats)
{
	return deserialize_uint64(page, stats->page_id_width);
}

int set_page_id_for_page(void* page, uint64_t page_id, const volatile_page_store_stats* stats)
{
	serialize_uint64(page, stats->page_id_width, page_id);
	return 1;
}

uint32_t get_system_header_size_for_page(uint64_t page_id, const volatile_page_store_stats* stats)
{
	if(is_free_space_mapper_page(page_id, stats))
		return 0;
	else
		return stats->page_id_width;
}

uint32_t get_page_content_size_for_page(uint64_t page_id, const volatile_page_store_stats* stats)
{
	return stats->page_size - get_system_header_size_for_page(page_id, stats);
}

uint32_t get_system_header_size_for_data_pages(const volatile_page_store_stats* stats)
{
	return stats->page_id_width;
}

uint32_t get_page_content_size_for_data_pages(const volatile_page_store_stats* stats)
{
	return stats->page_size - get_system_header_size_for_data_pages(stats);
}

uint32_t get_page_content_size_for_free_space_mapper_pages(const volatile_page_store_stats* stats)
{
	return stats->page_size;
}

void* get_page_contents_for_page(void* page, uint64_t page_id, const volatile_page_store_stats* stats)
{
	return page + get_system_header_size_for_page(page_id, stats);
}

void* get_page_for_page_contents(void* page_contents, uint64_t page_id, const volatile_page_store_stats* stats)
{
	return page_contents - get_system_header_size_for_page(page_id, stats);
}