#include <mm/buddy.h>
#include <mm/pmm.h>  // Assume kmalloc/kfree are available for kernel memory allocations
#include <string.h>
#include <printf.h>  // For kprintf debugging

/*
 * buddy_init():
 *   Initialize the buddy allocator to manage pages from [base, total_pages).
 *   'total_pages' is the total number of physical pages,
 *   and 'base' is the first usable page index.
 */
void buddy_init(buddy_allocator_t *buddy, uint64_t total_pages, uint64_t base) {
    if (total_pages <= base) {
        buddy->total_pages = 0;
        buddy->max_order = 0;
        buddy->free_lists = 0;
        return;
    }

    buddy->base = base;
    buddy->total_pages = total_pages - base; // Only manage usable pages.
    buddy->max_order = 0;
    while ((1ULL << (buddy->max_order + 1)) <= buddy->total_pages)
        buddy->max_order++;

    buddy->free_lists = kmalloc(sizeof(buddy_block_t *) * (buddy->max_order + 1));
    memset(buddy->free_lists, 0, sizeof(buddy_block_t*) * (buddy->max_order + 1));

    // Partition the usable pages into aligned buddy blocks.
    int relative_index = 0; // Start from 0 (relative to base)
    int remaining = buddy->total_pages;

    for (int order = buddy->max_order; order >= 0; order--) {
        int block_size = 1 << order;
        while (remaining >= block_size) {
            // Align the current index to block_size boundary
            if ((relative_index % block_size) != 0) {
                int align_skip = block_size - (relative_index % block_size);
                relative_index += align_skip;
                remaining -= align_skip;

                // If not enough pages left after aligning, break
                if (remaining < block_size)
                    break;
            }

            // Allocate a new block and link it into the free list
            buddy_block_t *block = kmalloc(sizeof(buddy_block_t));
            block->index = relative_index;
            block->next = buddy->free_lists[order];
            buddy->free_lists[order] = block;

            relative_index += block_size;
            remaining -= block_size;
        }
    }
}

/*
 * buddy_alloc():
 *   Allocates a block of 2^order contiguous pages.
 *   Returns the absolute starting page index (i.e. relative index + base),
 *   or -1 if no block is available.
 */
int buddy_alloc(buddy_allocator_t *buddy, int order) {
    for (int current_order = order; current_order <= buddy->max_order; current_order++) {
         if (buddy->free_lists[current_order] != NULL) {
             buddy_block_t *block = buddy->free_lists[current_order];
             buddy->free_lists[current_order] = block->next;
             int relative_index = block->index;
             kfree(block);
             while (current_order > order) {
                 current_order--;
                 int buddy_index = relative_index + (1 << current_order);
                 buddy_block_t *new_block = kmalloc(sizeof(buddy_block_t));
                 new_block->index = buddy_index;
                 new_block->next = buddy->free_lists[current_order];
                 buddy->free_lists[current_order] = new_block;
             }
             return relative_index + buddy->base;  // Return absolute index.
         }
    }
    return -1;
}

/*
 * buddy_free():
 *   Frees a block that was allocated at the given absolute starting page index
 *   with size 2^order pages.
 *   (Note: This simple implementation does not merge buddy blocks.)
 */
void buddy_free(buddy_allocator_t *buddy, int start_index, int order) {
   // Convert the absolute index to a relative index.
   int relative_index = start_index - buddy->base;
   buddy_block_t *block = kmalloc(sizeof(buddy_block_t));
   block->index = relative_index;
   block->next = buddy->free_lists[order];
   buddy->free_lists[order] = block;
}

void buddy_debug_print(buddy_allocator_t *buddy) {
    kprintf("Buddy Allocator Debug Print:\n");
    kprintf("Base page index: %llu\n", buddy->base);
    kprintf("Total pages:     %llu\n", buddy->total_pages);
    kprintf("Max order:       %d\n", buddy->max_order);

    // Step 1: Count how many blocks actually exist
    size_t block_count = 0;
    for (int order = 0; order <= buddy->max_order; order++) {
        buddy_block_t *cur = buddy->free_lists[order];
        while (cur) {
            block_count++;
            cur = cur->next;
        }
    }

    if (block_count == 0) {
        kprintf("No free blocks.\n");
        return;
    }

    // Step 2: Allocate compact list
    typedef struct {
        uint64_t abs_index;
        uint64_t size_in_pages;
        int order;
    } debug_block_info_t;

    debug_block_info_t *blocks = kmalloc(sizeof(debug_block_info_t) * block_count);
    if (!blocks) {
        kprintf("Failed to allocate debug block info array.\n");
        return;
    }

    // Step 3: Populate entries
    size_t i = 0;
    for (int order = 0; order <= buddy->max_order; order++) {
        uint64_t block_size = 1ULL << order;
        buddy_block_t *cur = buddy->free_lists[order];
        while (cur) {
            blocks[i].abs_index = cur->index + buddy->base;
            blocks[i].size_in_pages = block_size;
            blocks[i].order = order;
            cur = cur->next;
            i++;
        }
    }

    // Step 4: Sort by abs_index (bubble sort for small N; use qsort if libc present)
    for (size_t x = 0; x < block_count; x++) {
        for (size_t y = x + 1; y < block_count; y++) {
            if (blocks[y].abs_index < blocks[x].abs_index) {
                debug_block_info_t tmp = blocks[x];
                blocks[x] = blocks[y];
                blocks[y] = tmp;
            }
        }
    }

    // Step 5: Print sorted free blocks
    kprintf("\nFree Memory Blocks (sorted by physical address):\n");
    for (size_t j = 0; j < block_count; j++) {
        uint64_t start_page = blocks[j].abs_index;
        uint64_t end_page   = start_page + blocks[j].size_in_pages - 1;
        uint64_t start_addr = start_page * PAGE_SIZE;
        uint64_t end_addr   = (end_page + 1) * PAGE_SIZE - 1;

        kprintf("Order %-2d | Pages: %-6llu | [0x%012llx - 0x%012llx] | Pages [%llx - %llx] | %llu MB\n",
                blocks[j].order,
                blocks[j].size_in_pages,
                start_addr, end_addr,
                start_page, end_page,
                (blocks[j].size_in_pages * PAGE_SIZE)/(1024*1024));
    }

    // Step 6: Print total free memory summary
    uint64_t total_free_pages = 0;
    for (size_t j = 0; j < block_count; j++)
        total_free_pages += blocks[j].size_in_pages;

    kprintf("\nTotal Free Pages: %llu (%llu MiB)\n",
            total_free_pages,
            (uint64_t)((total_free_pages * PAGE_SIZE) / (1024.0 * 1024.0)));

    kfree(blocks);
}

bool buddy_is_page_free(buddy_allocator_t *buddy, uint64_t phys_addr) {
    uint64_t page_index = phys_addr / PAGE_SIZE;
    uint64_t rel_index = page_index - buddy->base;

    for (int order = 0; order <= buddy->max_order; order++) {
        uint64_t block_size = 1ULL << order;
        buddy_block_t *cur = buddy->free_lists[order];
        while (cur) {
            uint64_t start = cur->index + buddy->base;
            uint64_t end = start + block_size - 1;
            if (page_index >= start && page_index <= end)
                return true;
            cur = cur->next;
        }
    }
    return false;
}

int buddy_alloc_at(buddy_allocator_t *buddy, int order, uint64_t desired_phys_addr) {
    if (desired_phys_addr % PAGE_SIZE != 0)
        return -1; // must be aligned

    uint64_t desired_index = desired_phys_addr / PAGE_SIZE;
    if (desired_index < buddy->base)
        return -1;

    uint64_t relative_index = desired_index - buddy->base;

    // Must be aligned to 2^order
    if ((relative_index % (1ULL << order)) != 0)
        return -1;

    // First: Check if exact block exists in order
    buddy_block_t **prev = &buddy->free_lists[order];
    buddy_block_t *cur = buddy->free_lists[order];

    while (cur) {
        if (cur->index == relative_index) {
            // Found exact block — allocate it
            *prev = cur->next;
            kfree(cur);
            return desired_index;
        }
        prev = &cur->next;
        cur = cur->next;
    }

    // Second: Try to split larger block that includes it
    for (int higher_order = order + 1; higher_order <= buddy->max_order; higher_order++) {
        buddy_block_t **p = &buddy->free_lists[higher_order];
        buddy_block_t *b = buddy->free_lists[higher_order];

        while (b) {
            uint64_t block_index = b->index;
            uint64_t block_size = 1ULL << higher_order;

            // Check if this larger block can be split into target block
            if (relative_index >= block_index &&
                relative_index < block_index + block_size &&
                ((relative_index - block_index) % (1ULL << order) == 0)) {

                // Remove from list
                *p = b->next;
                kfree(b);

                // Split down to desired order
                uint64_t current_index = block_index;
                for (int split_order = higher_order - 1; split_order >= order; split_order--) {
                    uint64_t half_size = 1ULL << split_order;
                    if (relative_index < current_index + half_size) {
                        // Desired block is in the left half.
                        // Add the right half to free list.
                        buddy_block_t *right = kmalloc(sizeof(buddy_block_t));
                        right->index = current_index + half_size;
                        right->next = buddy->free_lists[split_order];
                        buddy->free_lists[split_order] = right;
                        // current_index remains unchanged.
                    } else {
                        // Desired block is in the right half.
                        // Add the left half to free list.
                        buddy_block_t *left = kmalloc(sizeof(buddy_block_t));
                        left->index = current_index;
                        left->next = buddy->free_lists[split_order];
                        buddy->free_lists[split_order] = left;
                        current_index = current_index + half_size;
                    }
                }
/*
                for (int split_order = higher_order - 1; split_order >= order; split_order--) {
                    uint64_t half_size = 1ULL << split_order;

                    buddy_block_t *right = kmalloc(sizeof(buddy_block_t));
                    right->index = current_index + half_size;
                    right->next = buddy->free_lists[split_order];
                    buddy->free_lists[split_order] = right;

                    if (relative_index < current_index + half_size) {
                        // Keep left half
                        current_index = current_index;
                    } else {
                        // Keep right half
                        current_index = current_index + half_size;
                    }
                }
                    */

                return current_index + buddy->base; // final block index (absolute)
            }

            p = &b->next;
            b = b->next;
        }
    }

    return -1; // Not found
}
