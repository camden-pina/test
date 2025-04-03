#ifndef _PMM_H
#define _PMM_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <queue.h>

#include <mm_types.h>

// TODO: switch to better allocator for large sizes
#define CHUNK_MIN_SIZE   8
#define CHUNK_MAX_SIZE   (KERNEL_HEAP_SIZE - sizeof(mm_chunk_t)) //  524288
#define CHUNK_SIZE_ALIGN 8
#define CHUNK_MIN_ALIGN  16

#define CHUNK_MAGIC 0xC0DE
#define HOLE_MAGIC 0xDEAD

typedef struct mm_chunk {
    uint16_t magic;                   // magic number
    uint16_t prev_offset;             // offset to previous chunk
    uint32_t size : 31;               // size of chunk
    uint32_t free : 1;                // chunk free/used
    LIST_ENTRY(struct mm_chunk) list; // links to free chunks (if free)
  } mm_chunk_t;
  static_assert(sizeof(mm_chunk_t) == 24);

typedef struct mm_heap {
    uintptr_t phys_addr;          // physical address of heap
    uintptr_t virt_addr;          // virtual address of heap base
    mm_chunk_t *last_chunk;       // the last created chunk
    LIST_HEAD(mm_chunk_t) chunks; // linked list of free chunks
  
    size_t size;                  // the size of the heap
    size_t used;                  // the total number of bytes used
    struct {
      size_t alloc_count;         // the number of times malloc was called
      size_t free_count;          // the number of times free was called
      size_t alloc_sizes[9];      // a histogram of alloc request sizes
    } stats;
  } mm_heap_t;
  
  typedef uintptr_t dma_addr_t;

void init_kheap();

void pmm_init(void* mMap, size_t mMapSize, size_t mMapDescSize);

// void* kmalloc(size_t sz);
void* krealloc(void* ptr, size_t sz);
// void kfree(void* ptr);

void *kmalloc(size_t size);
void *kmallocz(size_t size);
void *kmalloca(size_t size, size_t alignment);
void kfree(void *ptr);

uintptr_t pmm_early_alloc_pages(size_t count);
void *pmm_alloc(void);
void *dma_alloc_coherent(size_t size, dma_addr_t *dma_handle);
page_t *alloc_pages_at(uint64_t phys_addr, int count, size_t page_size);
page_t *alloc_pages(size_t count);

void print_buddy_debug();

uintptr_t kheap_phys_addr(void);

__ref page_t *alloc_cow_pages(page_t *pages);

void print_kheap_info(void);

void print_all_heap_blocks(void);

#endif // _PMM_H