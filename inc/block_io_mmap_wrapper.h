#ifndef BLOCK_IO_MMAP_WRAPPER_H
#define BLOCK_IO_MMAP_WRAPPER_H

#include<block_io.h>

// below two functions directly use mmap and munmap to get pages for you, to work on
// they only ensure that the checksum is valid

// returns mmap-ed page or crashes
// crashes if page_id is invalid/out of bounds OR if checksum does not validate
void* block_io_get_page(volatile_page_store* vps, uint64_t page_id);

// returns the mmap-ed page back to the OS, after setting the checksum
void block_io_return_page(volatile_page_store* vps, void* page);

#endif