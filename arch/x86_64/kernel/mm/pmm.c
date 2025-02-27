#include <mm/pmm.h>
#include <printf.h>
#include <panic.h>

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
	EfiLoaderDate,
	EfiBootServicesCode,
	EfiBootServicesDate,
	EfiRuntieServicesCode,
	EfiRuntimeServicesData,
	EfiConventionalMemory,
	EfiUnusableMemory,
	EfiACPIReclaimMemory,
	EfiACPIMemoryNVS,
	EfiMemoryMappedIO,
	EfiMemmoryMappedIOPortSpace,
	EfiPalCode,
	EfiMaxMemoryType
} EFI_MEMORY_TYPE;

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

void pmm_init(void* memoryMap, uint64_t memoryMapSize, uint64_t memoryMapDescSize)
{
    kprintf("memory map size: %llu", memoryMapSize);
    total_memory = 0;
    total_memory_free = 0;
    total_memory_used = 0;
    total_memory_reserved = 0;

    void* largestFreeMemorySegment = NULL;
    uint64_t largestFreeMemorySegmentSize = 0;


    uint64_t memoryMapEntries = memoryMapSize / sizeof(memory_map_entry_t); // (memoryMapSize / memoryMapDescSize);

    // drawRect(0, 0, 1366, 768, 0x00000000);
    kprintf("raw");

    for (uint64_t i = 0; i < memoryMapEntries; i++)
    {
        EFI_MEMORY_DESCRIPTOR* descriptor = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)memoryMap + (i * memoryMapDescSize));

        total_memory += descriptor->numberOfPages * 0x1000;

        if (descriptor->type == 7)
        {   
            if ((descriptor->numberOfPages * 0x1000) > largestFreeMemorySegmentSize)
            {
                largestFreeMemorySegment = descriptor->physicalStart;
                largestFreeMemorySegmentSize = descriptor->numberOfPages * 0x1000;
            }
        }
    }
    
    total_memory_free = total_memory;
    
    uint64_t bitmapSize = ((total_memory / 0x1000) / 8) + 1;
    bitmap_init(bitmapSize, largestFreeMemorySegment);

    pmm_pages_lock(bitmap_start, (bitmapSize / 0x1000) + 1);

    for (uint64_t i = 0; i < memoryMapEntries; i++)
    {
        EFI_MEMORY_DESCRIPTOR* descriptor = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)memoryMap + (i * memoryMapDescSize));
        if (descriptor->type != 7)
        {
            pmm_pages_reserve(descriptor->physicalStart, descriptor->numberOfPages);
        }
    }
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
void* kmalloc(size_t sz)
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
