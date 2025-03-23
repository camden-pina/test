#ifndef BUDDY_H
#define BUDDY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Simple buddy block node for our free lists */
typedef struct buddy_block {
    int index;                  // Relative starting page index (relative to the base)
    struct buddy_block *next;
} buddy_block_t;

/* Buddy allocator structure to manage physical pages.
   'total_pages' is the total number of physical pages,
   'base' is the first usable page index (i.e. lowest usable physical address / PAGE_SIZE),
   and the allocator manages pages from [base, total_pages).
*/
typedef struct {
    uint64_t total_pages;       // Number of physical pages managed (usable pages only)
    uint64_t base;              // First usable page index
    int max_order;              // Maximum order such that block size = 2^order pages
    buddy_block_t **free_lists; // Array (of size max_order+1) of free lists for each order
} buddy_allocator_t;

/* Initialize the buddy allocator for pages [base, total_pages).
   'total_pages' is the total number of physical pages,
   and 'base' is the first usable page index.
*/
void buddy_init(buddy_allocator_t *buddy, uint64_t total_pages, uint64_t base);

/* Allocate a block of 2^order contiguous pages.
   Returns the absolute starting page index (relative index + base)
   or -1 on failure.
*/
int buddy_alloc(buddy_allocator_t *buddy, int order);

/* Free a block starting at the given absolute page index with size 2^order pages.
   (Note: For simplicity, merging is not fully implemented.)
*/
void buddy_free(buddy_allocator_t *buddy, int start_index, int order);

void buddy_debug_print(buddy_allocator_t *buddy);
int buddy_alloc_at(buddy_allocator_t *buddy, int order, uint64_t desired_phys_addr);

#endif // BUDDY_H
