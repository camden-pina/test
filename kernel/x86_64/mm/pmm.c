#include <mm/pmm.h>
#include <printf.h>
#include <panic.h>
#include <kernel.h>
#include <mm/vmem.h>
#include <mm/pgtable.h>
#include <queue.h>
#include <string.h>
#include <mm/buddy.h>
#include <panic.h>

/* EFI memory type strings */
const char* EFI_MEMORY_TYPE_STRINGS[] =
{
    "EfiReservedMemoryType",
    "EfiLoaderCode",
    "EfiLoaderData",
    "EfiBootServicesCode",
    "EfiBootServicesData",
    "EfiRuntimeServicesCode",
    "EfiRuntimeServicesData",
    "EfiConventionalMemory",       // Free Memory
    "EfiUnusableMemory",
    "EfiACPIReclaimedMemory",      // Free Memory After Using ACPI Tables
    "EfiACPIMemoryNVM",
    "EfiMemmoryMappedIO",
    "EfiMemoryMappedIOPortSapce",
    "EfiPalCode",          
};

typedef enum
{
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/* Memory map definitions */
#define MEMORY_UNKNOWN          0
#define MEMORY_UNUSABLE         1
#define MEMORY_USABLE           2
#define MEMORY_RESERVED         3
#define MEMORY_ACPI             4
#define MEMORY_ACPI_NVS         5
#define MEMORY_MAPPED_IO        6
#define MEMORY_EFI_RUNTIME_CODE 7
#define MEMORY_EFI_RUNTIME_DATA 8

typedef struct EFI_MEMORY_DESCRIPTOR
{
    uint32_t type;
    void* physicalStart;
    void* virtualStart;
    uint64_t numberOfPages;
    uint64_t attribute;
} EFI_MEMORY_DESCRIPTOR;

uint64_t total_memory;      // total memory size in bytes
uint64_t total_memory_free;
uint64_t total_memory_used;
uint64_t total_memory_reserved;

uintptr_t kernel_reserved_start;
uintptr_t kernel_reserved_end;
uintptr_t kernel_reserved_ptr;
memory_map_entry_t *reserved_map_entry;
uintptr_t kernel_reserved_va_ptr;

#define END_ADDR(heap) ((heap)->virt_addr + (heap)->size)

static const char *hist_labels[9] = {
    "0-8", "9-16", "17-32", "33-64", "65-128", "129-512", "513-1024", "larger"
};

static inline int get_hist_bucket(size_t size) {
    switch (size) {
        case 1 ... 8: return 0;
        case 9 ... 16: return 1;
        case 17 ... 32: return 2;
        case 33 ... 64: return 3;
        case 65 ... 128: return 4;
        case 129 ... 512: return 5;
        case 513 ... 1024: return 6;
        default: return 7;
    }
}

static inline mm_chunk_t *get_prev_chunk(mm_chunk_t *chunk) {
    if (chunk->prev_offset == 0) {
        return NULL;
    }
    mm_chunk_t *prev = offset_ptr(chunk, -chunk->prev_offset);
    if (prev->magic != CHUNK_MAGIC) {
        panic("[get_prev_chunk] chunk magic is invalid");
    }
    return prev;
}

static inline mm_chunk_t *get_next_chunk(mm_heap_t *heap, mm_chunk_t *chunk) {
    if (chunk == heap->last_chunk) {
        return NULL;
    }
    uintptr_t next_addr = offset_addr(chunk, sizeof(mm_chunk_t) + chunk->size);
    if (next_addr < END_ADDR(heap) && ((uint16_t *) next_addr)[0] == HOLE_MAGIC) {
        uint16_t hole_size = ((uint16_t *) next_addr)[1];
        next_addr += hole_size;
    }
    if (next_addr >= END_ADDR(heap)) {
        return NULL;
    }
    mm_chunk_t *next = (void *)next_addr;
    if (next->magic != CHUNK_MAGIC) {
        panic("[get_next_chunk] chunk magic is invalid");
    }
    return next;
}

/* --- Global Buddy Allocator --- */
static buddy_allocator_t global_buddy;  // used for buddy-style physical allocations

/*
 * pmm_init():
 *  Initializes the physical memory manager.
 *  Scans the memory map, sets up reserved regions, and then initializes the
 *  kernel heap. After the heap is ready (so kmalloc() works correctly),
 *  the buddy allocator is initialized over only the usable physical memory.
 *
 *  This version dynamically computes the buddy allocator’s base value from the memory map.
 */
void pmm_init(void* memoryMap, uint64_t memoryMapSize, uint64_t memoryMapDescSize) {
    kprintf("Initializing PMM with memory map size: %llu bytes\n", boot_info_v2->mem_map.size);

    uint64_t total_memory = 0;
    uint64_t total_memory_free = 0;
    uint64_t total_memory_used = 0;
    uint64_t total_memory_reserved = 0;

    void* largestFreeMemorySegment = NULL;
    uint64_t largestFreeMemorySegmentSize = 0;

    size_t usable_mem_size = 0;
    memory_map_entry_t *kernel_entry = NULL;
    memory_map_entry_t *kernel_reserved_entry = NULL;
    uintptr_t kernel_start_phys = boot_info_v2->kernel_phys_addr;
    uintptr_t kernel_end_phys = kernel_start_phys + boot_info_v2->kernel_size;

    memory_map_t *memory_map = &boot_info_v2->mem_map;
    uint64_t memoryMapEntries = memory_map->size / sizeof(memory_map_entry_t);
    kprintf("Total memory map entries: %llu\n", memoryMapEntries);

    // We'll also compute the lowest usable physical address.
    uintptr_t lowest_usable = (uintptr_t)(-1);
    
    for (uint64_t i = 0; i < memoryMapEntries; i++) {
        memory_map_entry_t *entry = &memory_map->map[i];
        size_t size = entry->size;
        uintptr_t start = entry->base;
        uintptr_t end = start + size;
        kprintf("Entry %llu: Start=%p, Type=%u\n", i, start, entry->type);
        const char *type = NULL;
        switch (entry->type) {
            case MEMORY_UNKNOWN: type = "unknown"; break;
            case MEMORY_UNUSABLE: type = "unusable"; break;
            case MEMORY_USABLE: type = "usable"; break;
            case MEMORY_RESERVED: type = "reserved"; break;
            case MEMORY_ACPI: type = "ACPI data"; break;
            case MEMORY_ACPI_NVS: type = "ACPI NVS"; break;
            case MEMORY_MAPPED_IO: type = "memory mapped io"; break;
            case MEMORY_EFI_RUNTIME_CODE: type = "EFI runtime code"; break;
            case MEMORY_EFI_RUNTIME_DATA: type = "EFI runtime data"; break;
            default: panic("bad memory map");
        }
        if (entry->type == MEMORY_USABLE) {
            usable_mem_size += size;
            if (start != 0 && start < lowest_usable) {
                lowest_usable = start;
            }
            if (kernel_reserved_entry == NULL && start >= SIZE_16MB && entry->size >= SIZE_8MB) {
                kernel_reserved_entry = entry;
                kernel_reserved_start = start;
                kernel_reserved_end = end;
                kernel_reserved_ptr = start;
                kernel_reserved_va_ptr = KERNEL_RESERVED_VA;
            }
        }
        if (kernel_start_phys >= start && kernel_end_phys <= end) {
            kassert(kernel_entry == NULL);
            kassert(kernel_start_phys == start);
            kernel_entry = entry;
        }
        kprintf("  [0x%x-0x%x] %s (%llx)\n", (uint64_t)start, (uint64_t)end, type, entry->size / PAGE_SIZE);
    }
    
    if (lowest_usable == (uintptr_t)(-1)) {
        panic("No usable memory found in the memory map!");
    }
    
    kprintf("total memory: %llu GB\n", boot_info_v2->mem_total / 1024 / 1024);
    kprintf("usable memory: %llu GB\n", usable_mem_size / 1024 /1024);
    kassert(kernel_entry != NULL);
    kassert(kernel_reserved_entry != NULL);
    kernel_entry->base = kernel_end_phys;
    kernel_entry->size -= boot_info_v2->kernel_size;
    reserved_map_entry = kernel_reserved_entry;
    kprintf("kernel_reserved_start: 0x%x\n", (uint64_t)kernel_reserved_start);
    kprintf("kernel_reserved_end: 0x%x\n", (uint64_t)kernel_reserved_end);

    /* Initialize the kernel heap first (so that kmalloc() works) */
    init_kheap();

    /* Now initialize the buddy allocator.
       Compute total number of pages from boot_info_v2->mem_total.
    */
    uint64_t total_pages = boot_info_v2->mem_total / PAGE_SIZE;
    // Compute the buddy base dynamically: lowest usable physical address divided by PAGE_SIZE.
    uint64_t buddy_base = lowest_usable / PAGE_SIZE;
    kprintf("Buddy allocator base (first usable page index) = %llu\n", buddy_base);
    kprintf("Buddy allocator total pages: %llx\n", total_pages);
    buddy_init(&global_buddy, total_pages, buddy_base);
}

/*
 * pmm_early_alloc_pages():
 *  Allocates 'count' pages using a bump allocator from the reserved region.
 *  (Used only early during boot.)
 */
uintptr_t pmm_early_alloc_pages(size_t count) {
    uintptr_t addr = kernel_reserved_ptr;
    size_t size = count * PAGE_SIZE;
    kernel_reserved_ptr += size;
    if (kernel_reserved_ptr > kernel_reserved_end) {
        panic("Out of reserved memory. Tried to allocate %llu pages at page size of %x", count, PAGE_SIZE);
    }
    reserved_map_entry->base = kernel_reserved_ptr;
    reserved_map_entry->size -= size;
    return addr;
}

void *mm_early_map_pages_reserved(uintptr_t phys_addr, size_t count, uint32_t vm_flags) {
    uintptr_t va_ptr = kernel_reserved_va_ptr;
    size_t size = PAGE_SIZE;
    if (vm_flags & VM_HUGE_2MB) {
      size = SIZE_2MB;
    } else if (vm_flags & VM_HUGE_1GB) {
      size = SIZE_1GB;
    }
    kernel_reserved_va_ptr += size;
    return early_map_entries(va_ptr, phys_addr, count, vm_flags);
  }

mm_heap_t kheap;

/*
 * init_kheap():
 *  Initializes the kernel heap by allocating physical pages using pmm_early_alloc_pages()
 *  and mapping them at a fixed virtual address.
 */
void init_kheap() {
    size_t page_count = SIZE_TO_PAGES(KERNEL_HEAP_SIZE);
    uintptr_t phys_addr = pmm_early_alloc_pages(page_count);
    uintptr_t virt_addr = KERNEL_HEAP_VA;
    if (KERNEL_HEAP_SIZE >= BIGPAGE_SIZE && is_aligned(phys_addr, BIGPAGE_SIZE)) {
        uintptr_t num_bigpages = KERNEL_HEAP_SIZE / BIGPAGE_SIZE;
        page_count = PAGES_TO_SIZE(page_count) % BIGPAGE_SIZE;
        early_map_entries(virt_addr, phys_addr, num_bigpages, VM_RDWR|VM_HUGE_2MB);
        phys_addr += num_bigpages * BIGPAGE_SIZE;
        virt_addr += num_bigpages * BIGPAGE_SIZE;
    }
    if (page_count > 0) {
        early_map_entries(virt_addr, phys_addr, page_count, VM_RDWR);
    }
    memset(&kheap, 0, sizeof(mm_heap_t));
    kheap.phys_addr = phys_addr;
    kheap.virt_addr = KERNEL_HEAP_VA;
    kheap.size = KERNEL_HEAP_SIZE;
    kheap.used = 0;
    kheap.last_chunk = NULL;
    LIST_INIT(&kheap.chunks);
    kprintf("initialized kernel heap\n");
}

/*
 * __kmalloc():
 *  Core allocation function that searches for a suitable free chunk or creates one.
 */
void *__kmalloc(mm_heap_t *heap, size_t size, size_t alignment) {
    kassert(heap != NULL);
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        panic("[kmalloc] invalid alignment given: %zu\n", alignment);
    }
    if (size == 0) {
        return NULL;
    } else if (size > CHUNK_MAX_SIZE) {
        panic("[kmalloc] error - request too large (%llu)\n", size);
    }
    heap->stats.alloc_count++;
    heap->stats.alloc_sizes[get_hist_bucket(size)]++;
    size = align(max(size, CHUNK_MIN_SIZE), CHUNK_SIZE_ALIGN);
    if (LIST_FIRST(&heap->chunks)) {
        mm_chunk_t *chunk = NULL;
        mm_chunk_t *curr = NULL;
        LIST_FOREACH(curr, &heap->chunks, list) {
            if (offset_addr(curr, sizeof(mm_chunk_t)) % alignment != 0) {
                continue;
            }
            if (curr->size >= size) {
                if (chunk == NULL || (curr->size < chunk->size)) {
                    chunk = curr;
                }
            } else if (curr->size == size) {
                chunk = curr;
                break;
            }
        }
        if (chunk != NULL) {
            LIST_REMOVE(&heap->chunks, chunk, list);
            chunk->free = false;
            heap->used += size + sizeof(mm_chunk_t);
            return offset_ptr(chunk, sizeof(mm_chunk_t));
        }
    }
    uintptr_t chunk_addr;
    if (heap->last_chunk == NULL) {
        chunk_addr = heap->virt_addr;
    } else {
        chunk_addr = offset_addr(heap->last_chunk, sizeof(mm_chunk_t) + heap->last_chunk->size);
    }
    uintptr_t aligned_mem = align(chunk_addr + sizeof(mm_chunk_t), alignment);
    uintptr_t aligned_chunk = aligned_mem - sizeof(mm_chunk_t);
    if (aligned_chunk != chunk_addr) {
        size_t hole_size = aligned_chunk - chunk_addr;
        if (hole_size < sizeof(mm_chunk_t) + CHUNK_MIN_SIZE) {
            ((uint16_t *) chunk_addr)[0] = HOLE_MAGIC;
            ((uint16_t *) chunk_addr)[1] = hole_size;
            heap->used += hole_size;
        } else {
            mm_chunk_t *free_chunk = (void *) chunk_addr;
            free_chunk->magic = CHUNK_MAGIC;
            free_chunk->size = hole_size - sizeof(mm_chunk_t);
            free_chunk->free = true;
            LIST_ADD_FRONT(&heap->chunks, free_chunk, list);
        }
    }
    if (aligned_mem + size > END_ADDR(heap)) {
        kprintf("heap: allocation overflows end of heap: %p (size=%zu, align=%zu)\n", aligned_mem, size, alignment);
        kprintf("heap: heap out of memory\n");
        kprintf("      virt_addr = %p\n", heap->virt_addr);
        kprintf("      size = %zu\n", heap->size);
        kprintf("      used = %zu\n", heap->used);
        kprintf("      alloc count = %zu\n", heap->stats.alloc_count);
        kprintf("      free count = %zu\n", heap->stats.free_count);
        kprintf("      request_sizes:\n");
        for (int i = 0; i < ARRAY_SIZE(hist_labels); i++) {
            kprintf("        %s - %zu\n", hist_labels[i], heap->stats.alloc_sizes[i]);
        }
        panic("[kmalloc] error - out of memory");
    }
    mm_chunk_t *chunk = (void *) aligned_chunk;
    chunk->magic = CHUNK_MAGIC;
    chunk->size = size;
    chunk->free = false;
    chunk->list.next = NULL;
    chunk->list.prev = NULL;
    if (heap->last_chunk != NULL) {
        chunk->prev_offset = aligned_chunk - (uintptr_t) heap->last_chunk;
    } else {
        chunk->prev_offset = 0;
    }
    heap->last_chunk = chunk;
    heap->used += size + sizeof(mm_chunk_t);
    return offset_ptr(chunk, sizeof(mm_chunk_t));
}

void *kmalloc(size_t size) {
    return __kmalloc(&kheap, size, CHUNK_MIN_ALIGN);
}

void *kmallocz(size_t size) {
    void *p = __kmalloc(&kheap, size, CHUNK_MIN_ALIGN);
    memset(p, 0, size);
    return p;
}

void *kmalloca(size_t size, size_t alignment) {
    return __kmalloc(&kheap, size, alignment);
}

void* krealloc(void* ptr, size_t sz) {
    if (!ptr)
        return kmalloc(sz);
    void* newptr = kmalloc(sz);
    memcpy(newptr, ptr, sz);
    kfree(ptr);
    return newptr;
}

static uintptr_t next_pmm_va = KERNEL_RESERVED_VA;

/*
 * pmm_alloc():
 *  Allocates one physical page using the early bump allocator.
 *  Maps the physical page at the next free virtual address in the reserved region.
 */
void *pmm_alloc(void) {
    uintptr_t paddr = pmm_early_alloc_pages(1);
    if (!paddr) {
        panic("pmm_alloc: out of physical pages!\n");
    }
    uintptr_t vaddr = next_pmm_va;
    const uint32_t vm_flags = VM_WRITE | VM_READ | VM_NOCACHE;  
    early_map_entries(vaddr, paddr, 1, vm_flags);
    memset((void*)vaddr, 0, PAGE_SIZE);
    next_pmm_va += PAGE_SIZE;
    return (void*)vaddr;
}

void __kfree(mm_heap_t *heap, void *ptr) {
    kassert(heap != NULL);
    if (ptr == NULL) {
        return;
    }
    heap->stats.free_count++;
    mm_chunk_t *chunk = offset_ptr(ptr, -sizeof(mm_chunk_t));
    if (chunk->magic != CHUNK_MAGIC) {
        kprintf("[kfree] invalid pointer\n");
        return;
    } else if (chunk->free) {
        kprintf("[kfree] freeing already freed chunk\n");
        return;
    }
    if (!(LIST_NEXT(chunk, list) == NULL && LIST_PREV(chunk, list) == NULL)) {
        panic("[kfree] error - chunk linked to other chunks");
    }
    chunk->free = true;
    mm_chunk_t *next_chunk = get_next_chunk(heap, chunk);
    if (next_chunk != NULL) {
        /* Optionally mark next chunk as having a free predecessor */
    }
    LIST_ADD_FRONT(&heap->chunks, chunk, list);
    heap->used -= chunk->size + sizeof(mm_chunk_t);
}

void kfree(void *ptr) {
    __kfree(&kheap, ptr);
}

/*
 * kheap_phys_addr():
 *  Returns the raw physical address of the kernel heap.
 */
uintptr_t kheap_phys_addr(void) {
    return kheap.phys_addr;
}

/*
 * dma_alloc_coherent():
 *  Allocates a contiguous block of physical memory for DMA and maps it
 *  at the next free virtual address in the reserved region.
 */
void *dma_alloc_coherent(size_t size, dma_addr_t *dma_handle) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t paddr = pmm_early_alloc_pages(pages);
    if (!paddr) {
        panic("dma_alloc_coherent: out of physical pages!");
    }
    uintptr_t vaddr = next_pmm_va;
    const uint32_t vm_flags = VM_WRITE | VM_READ | VM_NOCACHE;
    early_map_entries(vaddr, paddr, pages, vm_flags);
    memset((void *)vaddr, 0, pages * PAGE_SIZE);
    next_pmm_va += pages * PAGE_SIZE;
    if (dma_handle) {
        *dma_handle = paddr;
    }
    return (void *)vaddr;
}

page_t *alloc_pages_at(uint64_t phys_addr, int count, size_t page_size) {
  if (page_size != PAGE_SIZE || count <= 0)
      return NULL;

  if (phys_addr % PAGE_SIZE != 0)
      return NULL;

  // Ensure count is a power of two
  if ((count & (count - 1)) != 0)
      return NULL;

  // Convert count to order (i.e., log2(count))
  int order = 0;
  while ((1 << order) < count)
      order++;

  // Try to allocate the block at given address
  int result = buddy_alloc_at(&global_buddy, order, phys_addr);
  if (result == -1)
      return NULL;

  // Construct linked list of page_t structs
  page_t *head = NULL;
  page_t *prev = NULL;

  for (int i = 0; i < count; i++) {
      page_t *page = kmalloc(sizeof(page_t));
      if (!page) {
          // Roll back in case of error
          page_t *p = head;
          while (p) {
              page_t *next = p->next;
              kfree(p);
              p = next;
          }
          return NULL;
      }

      page->address = phys_addr + (i * PAGE_SIZE);
      page->flags = 0;
      page->entries = NULL;
      page->next = NULL;

      if (i == 0) {
          // Mark head info
          page->head.count = count;
          page->head.contiguous = 1;
      }

      if (prev)
          prev->next = page;
      else
          head = page;

      prev = page;
  }

  return head;
}


/*
 * alloc_pages_size():
 *  Allocate a contiguous block of physical pages (each of the given page size)
 *  using the buddy allocator. This function computes the minimum order needed,
 *  calls buddy_alloc(), and builds an array of page_t structures.
 */
page_t *alloc_pages_size(size_t count, size_t pagesize) {
    int order = 0;
    while ((1 << order) < count) {
        order++;
    }
    int start_index = buddy_alloc(&global_buddy, order);
    if (start_index == -1) {
        panic("alloc_pages_size: unable to allocate a block of %zu pages", count);
    }
    uint64_t phys_addr = ((uint64_t)start_index * pagesize);
    page_t *pages = kmallocz(sizeof(page_t) * count);
    if (!pages) {
        panic("alloc_pages_size: failed to allocate memory for page structures");
    }
    for (size_t i = 0; i < count; i++) {
        pages[i].address = phys_addr + i * pagesize;
        pages[i].flags = 0;
        if (i == 0) {
            pages[i].head.count = count;
            pages[i].head.contiguous = (count > 1) ? 1 : 0;
        } else {
            pages[i].head.count = 0;
            pages[i].head.contiguous = 0;
        }
        pages[i].entries = NULL;
        pages[i].next = NULL;
    }
    return pages;
}

page_t *alloc_pages(size_t count) {
    return alloc_pages_size(count, PAGE_SIZE);
}

/*
 * alloc_cow_structs():
 *  Creates a new linked list of page_t structures for copy-on-write.
 *  Each new structure is allocated, its address and flags adjusted,
 *  and its source is set as a reference to the original page.
 */
__ref static page_t *alloc_cow_structs(page_t *pages) {
  kassert(pages->flags & PG_HEAD);

  page_t *first = NULL;
  page_t *last = NULL;
  page_t *curr = pages;
  while (curr) {
    page_t *page = kmallocz(sizeof(page_t));
    page->address = curr->address;
    page->flags = (curr->flags & PG_SIZE_MASK) | PG_COW;
    page->source = getref(curr);
    initref(page);
    if (first == NULL) {
      first = moveref(page);
      last = first;
    } else {
      last->next = moveref(page);
      last = last->next;
    }

    curr = curr->next;
  }

  first->flags |= PG_HEAD;
  first->head.count = pages->head.count;
  first->head.contiguous = pages->head.contiguous;
  return moveref(first);
}

__ref page_t *alloc_cow_pages(page_t *pages) {
    return alloc_cow_structs(pages);
  }

void print_buddy_debug() {
  buddy_debug_print(&global_buddy);
}
