#ifndef _PMM_H
#define _PMM_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <mm/bitmap.h>

void pmm_init(void* mMap, size_t mMapSize, size_t mMapDescSize);

void* kmalloc(size_t sz);
void* krealloc(void* ptr, size_t sz);
void kfree(void* ptr);

void pmm_pages_unreserve(void* address, uint64_t pageCount);
void pmm_pages_reserve(void* address, uint64_t pageCount);
void pmm_pages_free(void* address, uint64_t pageCount);
void pmm_pages_lock(void* address, uint64_t pageCount);

uint64_t pmm_get_total_memory(void);
uint64_t pmm_get_total_memory_used(void);
uint64_t pmm_get_total_memory_free(void);
uint64_t pmm_get_total_memory_reserved(void);

#endif // _PMM_H