#ifndef VOLATILE_PAGE_STORE_H
#define VOLATILE_PAGE_STORE_H

#include<block_io.h>

typedef struct volatile_page_store volatile_page_store;
struct volatile_page_store
{
	block_file temp_block_file;
};

#endif