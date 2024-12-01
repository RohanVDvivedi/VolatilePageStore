#include<volatile_page_store_stats.h>

#include<block_io.h>

#include<system_page_header_util.h>

volatile_page_store_user_stats get_volatile_page_store_user_stats(const volatile_page_store_stats* stats, uint32_t temp_file_block_size)
{
	// max_page_count is min(MAX_PAGE_COUNT_possible, 1 << (8 * page_id_width))
	// it is either dictated by the overflow of off_t or the page_id_width
	uint64_t max_page_count = ((MAX_BLOCK_COUNT(temp_file_block_size)) / (stats->page_size / temp_file_block_size));
	if(stats->page_id_width < 8)
		max_page_count = min(max_page_count, UINT64_C(1) << (CHAR_BIT * stats->page_id_width));

	return (volatile_page_store_user_stats){
		.page_size = get_page_content_size_for_data_pages(stats),
		.page_id_width = stats->page_id_width,
		.NULL_PAGE_ID = 0,
		.max_page_count = max_page_count,
	};
}