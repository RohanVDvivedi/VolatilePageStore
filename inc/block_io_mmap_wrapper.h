#ifndef BLOCK_IO_MMAP_WRAPPER_H
#define BLOCK_IO_MMAP_WRAPPER_H

#include<volatile_page_store.h>

// below two functions directly use mmap and munmap to get pages for you, to work on
// they only ensure that the checksum is valid

// returns mmap-ed page or crashes
// crashes also if checksum does not validate
void* block_io_get_page(volatile_page_store* vps, uint64_t page_id);

// returns the mmap-ed page back to the OS, after setting the checksum
void block_io_return_page(volatile_page_store* vps, void* page);

// ftruncates the database to expand it and then sets checksum on it
void expand_file_with_zero_page(volatile_page_store* vps, uint64_t* page_id);

#endif