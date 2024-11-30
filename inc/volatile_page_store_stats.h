#ifndef VOLATILE_PAGE_STORE_STATS_H
#define VOLATILE_PAGE_STORE_STATS_H

#include<stdint.h>

typedef struct volatile_page_store_stats volatile_page_store_stats;
struct volatile_page_store_stats
{
	uint32_t page_size;	// size of page in bytes
	uint32_t page_id_width; // bytes required to store page_id
};

typedef struct volatile_page_store_user_stats volatile_page_store_user_stats;
struct volatile_page_store_user_stats
{
	uint32_t page_size;	// size of page in bytes available to the user, effectively page_content_size for non free space mapper pages
	uint32_t page_id_width; // bytes required to store page_id, same as volatile_page_store_stats.page_id_width

	uint64_t NULL_PAGE_ID; // zero value, never access this page, ideally never access any page not allocated by the volatile_page_store, it will result in corruption

	uint64_t max_page_count; // user is not allowed to access more than this number of pages in the volatile_page_store
};

#endif