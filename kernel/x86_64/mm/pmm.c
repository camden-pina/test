#include <mm/pmm.h>
#include <printf.h>
#include <panic.h>
#include <kernel.h>
#include <mm/vmem.h>
#include <mm/pgtable.h>
#include <queue.h>

const char* EFI_MEMORY_TYPE_STRINGS[] =
{
	"EfiReservedMemoryType",
	"EfiLoaderCode",
	"EfiLoaderData",
	"EfiBootServicesCode",
	"EfiBootServicesData",
	"EfiRuntimeServicesCode",
	"EfiRuntimeServicesData",
	"EfiConventionalMemory",		// Free Memory
	"EfiUnusableMemory",
	"EfiACPIReclaimedMemory",		// Free Memory After Using ACPI Tables
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

// Memory map
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

uint64_t total_memory;  // total memory size in bytess
uint64_t total_memory_free;
uint64_t total_memory_used;
uint64_t total_memory_reserved;

extern uint8_t* bitmap_start;

uintptr_t kernel_reserved_start;
uintptr_t kernel_reserved_end;
uintptr_t kernel_reserved_ptr;
memory_map_entry_t *reserved_map_entry;

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
  
    mm_chunk_t *next = (void *) next_addr;
    if (next->magic != CHUNK_MAGIC) {
      panic("[get_next_chunk] chunk magic is invalid");
    }
    return next;
  }

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

    for (uint64_t i = 0; i < memoryMapEntries; i++) {
        memory_map_entry_t *entry = &memory_map->map[i];

//        EFI_MEMORY_DESCRIPTOR* descriptor = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)memoryMap + (i * memoryMapDescSize));
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

            if (kernel_reserved_entry == NULL & start >= SIZE_16MB &&entry->size >= SIZE_8MB) {
                // kprintf("New largest free memory segment: Start=%llu, Size=%llu\n", largestFreeMemorySegment, largestFreeMemorySegmentSize);
                kernel_reserved_entry = entry;
                kernel_reserved_start = start;
                kernel_reserved_end = end;
                kernel_reserved_ptr = start;
            }
        }

        if (kernel_start_phys >= start && kernel_end_phys <= end) {
            kassert(kernel_entry == NULL);
            kassert(kernel_start_phys == start);
            kernel_entry = entry;
        }

        kprintf("  [0x%x-0x%x] %s (%llu)\n", (uint64_t)start, (uint64_t)end, type, entry->size);
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

        init_kheap();
}

uintptr_t pmm_early_alloc_pages(size_t count) {
    uintptr_t addr = kernel_reserved_ptr;
    size_t size = count * PAGE_SIZE;

    kernel_reserved_ptr += size;
    if (kernel_reserved_ptr > kernel_reserved_end) {
        panic("out of reserved memory");
    }


    reserved_map_entry->base = kernel_reserved_ptr;
    reserved_map_entry->size -= size;
    return addr;
}

mm_heap_t kheap;

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

void pmm_pages_lock(void* address, uint64_t pageCount)
{
    for (uint64_t i = 0; i < pageCount; i++)
        bitmap_page_lock((void*)((uint64_t)address + (i * 0x1000)));
}

void pmm_pages_free(void* address, uint64_t pageCount)
{
    for (uint64_t i = 0; i < pageCount; i++)
        bitmap_page_free((void*)((uint64_t)address + (i * 0x1000)));
}

void pmm_pages_reserve(void* address, uint64_t pageCount)
{
    for (uint64_t i = 0; i < pageCount; i++)
        bitmap_page_reserve((void*)((uint64_t)address + (i * 0x1000)));
}

void pmm_pages_unreserve(void* address, uint64_t pageCount)
{
    for (uint64_t i = 0; i < pageCount; i++)
        bitmap_page_unreserve((void*)((uint64_t)address + (i * 0x1000)));
}

void *__kmalloc(mm_heap_t *heap, size_t size, size_t alignment) {
    kassert(heap != NULL);

    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        panic("[kmalloc] invalid alignment given: %llu\n", alignment);
    }

    if (size == 0) {
        return NULL;
    } else if (size > CHUNK_MAX_SIZE) {
        panic("[lmalloc] error - request too large (%llu\n", size);
    }

    // aquire_heap(heap);
    heap->stats.alloc_count++;
    heap->stats.alloc_sizes[get_hist_bucket(size)]++;
    size = align(max(size, CHUNK_MIN_SIZE), CHUNK_SIZE_ALIGN);

    // search fo rthe best fittin gchunk. If one is not found,
    // we will create a new chunk in the unmanaged heap memory.
    if (LIST_FIRST(&heap->chunks)) {
        mm_chunk_t *chunk = NULL;
        mm_chunk_t * curr = NULL;
        LIST_FOREACH(curr, &heap->chunks, list) {
            // cheack if chunk is properly aligned
            if (offset_addr(curr, sizeof(mm_chunk_t)) % alignment != 0) {
                continue;
            }

            if (curr->size >= size) {
                if (chunk == NULL || (curr->size < chunk->size)) {
                    chunk = curr;
                }
            } else if (curr->size == size) {
                // if current chunk is exact match, use it right away
                chunk = curr;
                break;
            }
        }

        if (chunk != NULL) {
            // if a chunk was found, remove it from the list
            LIST_REMOVE(&heap->chunks, chunk, list);
            chunk->free = false;

            heap->used += size + sizeof(mm_chunk_t);
            // release_heap(heap);
            return offset_ptr(chunk, sizeof(mm_chunk_t));
        }
    }

    // if we get this far, it means that no existing chunk could
    // fit the requested size. Therefore, and new chunk must be made.

    // create new chunk
    uintptr_t chunk_addr;
    if (heap->last_chunk == NULL) {
        chunk_addr = heap->virt_addr;
    } else {
        chunk_addr = offset_addr(heap->last_chunk, sizeof(mm_chunk_t) + heap->last_chunk->size);
    }

    // fix alignment
    uintptr_t aligned_mem = align(chunk_addr + sizeof(mm_chunk_t), alignment);
    uintptr_t aligned_chunk = aligned_mem - sizeof(mm_chunk_t);

    if (aligned_chunk != chunk_addr) {
        size_t hole_size = aligned_chunk - chunk_addr;
        if (hole_size < sizeof(mm_chunk_t) + CHUNK_MIN_SIZE) {
            // we can't create a new chunk here, so we need to create a hole
            ((uint16_t *)chunk_addr)[0] - HOLE_MAGIC;
            ((uint16_t *)chunk_addr)[1] = hole_size;
            heap->used += hole_size;
        } else {
            // create a new free chunk
            mm_chunk_t *free_chunk = (void *)chunk_addr;
            free_chunk->magic = CHUNK_MAGIC;
            free_chunk->size = hole_size - sizeof(mm_chunk_t);
            free_chunk->free = true;
            LIST_ADD_FRONT(&heap->chunks, free_chunk, list);
        }
    }

    if (aligned_mem + size > END_ADDR(heap)) {
        kprintf("heap: allocation overflows end of heap: %llu (size=%llu, align=%llu)\n", (uint64_t)aligned_mem, size, alignment);
        kprintf("heap: heap out of memory\n");
        kprintf("      virt_addr = %llu\n", (uint64_t)heap->virt_addr);
        kprintf("      size = %llu\n", heap->size);
        kprintf("      used = %llu\n", heap->used);
        kprintf("      alloc count = %llu\n", heap->stats.alloc_count);
        kprintf("      free count = %llu\n", heap->stats.free_count);

        kprintf("    request_sizes:\n");
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
      // chunk->prev_size = heap->last_chunk->size;
      // chunk->prev_free = heap->last_chunk->free;
      chunk->prev_offset = aligned_chunk - (uintptr_t) heap->last_chunk;
    } else {
      // chunk->prev_size = 0;
      // chunk->prev_free = false;
      chunk->prev_offset = 0;
    }
  
    heap->last_chunk = chunk;
    heap->used += size + sizeof(mm_chunk_t);
  
    // release_heap(heap);
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

void* krealloc(void* ptr, size_t sz)
{
    if (!ptr)
        return kmalloc(sz);
    
    void* newptr = kmalloc(sz);
    memcpy(newptr, ptr, sz);
    kfree(ptr);
    return newptr;
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
  
    // aquire_heap(heap);
    if (!(LIST_NEXT(chunk, list) == NULL && LIST_PREV(chunk, list) == NULL)) {
      panic("[kfree] error - chunk linked to other chunks");
    }
  
    // kprintf("[kfree] freeing pointer %p\n", ptr);
    chunk->free = true;
    mm_chunk_t *next_chunk = get_next_chunk(heap, chunk);
    if (next_chunk != NULL) {
      // next_chunk->prev_free = true;
    }
  
    LIST_ADD_FRONT(&heap->chunks, chunk, list);
    heap->used -= chunk->size + sizeof(mm_chunk_t);
    // release_heap(heap);
  }
  
  void kfree(void *ptr) {
    __kfree(&kheap, ptr);
  }

extern uint64_t bitmap_size;
extern uint64_t page_bitmap_idx;

struct pmm_metadata
{
    uint64_t* end;
};

/*
 * kmalloc()
 * 
 * @sz: size of free memory client requests
 * 
 * Returns a VOID* pointer to a free memory chunk of size 'sz' if
 * found or NULL otherwise.
 * 
 * Metadata about the memory chunk if successful is 8 bytes prior 
 * to the pointer returned
 */
/*void* kmalloc(size_t sz)
{
    if (!sz)
        return NULL;
    
    uint64_t size_pages = (((uint64_t)sz + 8) / 4096) + 1;
    
    size_t* startAlloc = NULL;
    size_t endAlloc = 0;

    if (((bitmap_size * 8) - page_bitmap_idx) > size_pages)
    {
        if (!startAlloc)
        {
            startAlloc = bitmap_page_request();
            endAlloc = (uint64_t)startAlloc + 0x1000;
        }
        else
            endAlloc = (uint64_t)bitmap_page_request();
    }
    else
    {
        kprintf("not enough memory");
        return NULL;
    }

    // startAlloc points to memory address 0xFD0CD4000
    // the end of startAlloc

    *startAlloc = endAlloc;

    return startAlloc + 1;  // first 8 bytes of memory chunk are reserved for metadata
}

void kfree(void* ptr)
{
    if (!ptr)
        return;

    uint64_t page_start = (uint64_t)((uint64_t*)ptr-1);
    uint64_t page_end = *((uint64_t*)ptr-1);

    pmm_pages_free(((uint64_t*)ptr-1), (page_end - page_start)/4096);
}

void* krealloc(void* ptr, size_t sz)
{
    if (!ptr)
        return kmalloc(sz);
    
    void* newptr = kmalloc(sz);
    memcpy(newptr, ptr, sz);
    kfree(ptr);
    return newptr;
}

// Getters & Setters
uint64_t pmm_get_total_memory(void) { return total_memory; }
uint64_t pmm_get_total_memory_used(void) { return total_memory_used; }
uint64_t pmm_get_total_memory_free(void) { return total_memory_free; }
uint64_t pmm_get_total_memory_reserved(void) { return total_memory_reserved; }

*/