#include <mm/vmem.h>
#include <kernel.h>
#include <string.h>
#include <8250.h>
#include <printf.h>

#include <mm/vmem.h>
#include <mm/pmm.h>
#include <mm/pgtable.h>
#include <kernel.h>
#include <panic.h>
#include <string.h>
#include <printf.h>

#include "queue.h"  // Include your queue definitions

/*
   This module implements a high-level virtual memory manager.
   It creates and maintains an address_space_t structure that holds
   a linked list of vm_mapping_t objects.
   
   Each mapping is created via an internal function vmap_internal() which,
   depending on the type (reserved, physical, or pages), uses lower-level
   mapping functions (like early_map_entries) to install the mapping into the page tables.
   
   Note: In this simple design the hint is used directly as the base address.
         In a more sophisticated design, you would search the address space
         for a free region (using an interval tree or similar data structure).
*/

/* Global address spaces and the current address space pointer.
   For now we create a kernel_space and a default_user_space. */
address_space_t *kernel_space = NULL;
address_space_t *default_user_space = NULL;
address_space_t *cur_space = NULL;

/* Helper macros for debugging */
#ifndef DEBUG_VM
#define DEBUG_VM 1
#endif

#if DEBUG_VM
#define dvm(fmt, ...) kprintf("[vmem] " fmt "\n", ##__VA_ARGS__)
#else
#define dvm(fmt, ...)
#endif

/*
 * Predefined kernel heap virtual address and size.
 * These should be defined elsewhere in your kernel (or in a header).
 */
#ifndef KERNEL_HEAP_VA
#define KERNEL_HEAP_VA 0xFFFFFF8000400000ULL
#endif

#ifndef KERNEL_HEAP_SIZE
#define KERNEL_HEAP_SIZE (6 * 1024 * 1024)  // 6MB heap
#endif

/*
 * Assume a helper function that returns the physical address of the kernel heap.
 * In our PMM module, for example, this might be implemented via pmm_early_alloc_pages.
 */
extern uintptr_t kheap_phys_addr(void);

/*
 * init_default_mappings()
 *
 * This function creates a set of default mappings:
 *
 * 1. A reserved null page at address 0 (to catch NULL dereferences).
 * 2. A fixed mapping for the kernel code and data.
 * 3. A fixed mapping for the kernel heap.
 * 4. A fixed mapping for the boot info structure.
 *
 * The virtual addresses are derived from the linker script symbols and predefined hints.
 */
void init_default_mappings(void) {
   
}

/* Helper: Insert mapping into the address space’s list. */
static void insert_vm_mapping(address_space_t *space, vm_mapping_t *vm) {
    LIST_ADD(&space->mappings, vm, list);
    space->num_mappings++;
}

/* Helper: Remove mapping from the list and free its memory. */
static void remove_vm_mapping(address_space_t *space, vm_mapping_t *vm) {
    LIST_REMOVE(&space->mappings, vm, list);
    space->num_mappings--;
    if (vm->name) {
        kfree(vm->name);
    }
    kfree(vm);
}

/* Helper function: Check if the region [start, start+size) is free */
static int is_region_free(address_space_t *space, uintptr_t start, size_t size) {
        vm_mapping_t *vm;
        LIST_FOREACH(vm, &space->mappings, list) {
            // Check if the region [start, start+size) overlaps with mapping [vm->address, vm->address+vm->size)
            if (!(start + size <= vm->address || start >= vm->address + vm->size)) {
                return 0; // Overlap found: region is not free
            }
        }
        return 1; // Region is free
    }
    
    /* Helper function: Find a free region of at least 'size' bytes starting at or after 'hint'
       Returns 0 if no free region is found. */
    static uintptr_t find_free_region(address_space_t *space, size_t size, uintptr_t hint) {
        // Ensure we start at least at the minimum address for the space.
        uintptr_t candidate = (hint < space->min_addr) ? space->min_addr : hint;
        
        // Loop until candidate + size exceeds the maximum allowed address.
        while (candidate + size <= space->max_addr) {
            if (is_region_free(space, candidate, size)) {
                return candidate;
            }
    
            /* Instead of simply incrementing by PAGE_SIZE, we check all mappings
               to see if candidate overlaps any mapping and then jump candidate to the end
               of that mapping to speed up the search. */
            vm_mapping_t *vm;
            int advanced = 0;
            LIST_FOREACH(vm, &space->mappings, list) {
                if (!(candidate + size <= vm->address || candidate >= vm->address + vm->size)) {
                    // Move candidate to the end of the overlapping mapping.
                    if (vm->address + vm->size > candidate) {
                        candidate = vm->address + vm->size;
                        advanced = 1;
                    }
                }
            }
            // If none of the mappings forced an advance, move candidate by one page.
            if (!advanced) {
                candidate += PAGE_SIZE;
            }
        }
        return 0; // No suitable region found
    }
    
    /*
     * Updated internal mapping function.
     * This version treats the provided 'hint' as a suggestion and searches
     * for a free region if the VM_FIXED flag is not set.
     */
    static int vmap_internal(address_space_t *space,
            enum vm_type type,
            uintptr_t hint,
            size_t size,
            size_t vm_size,
            uint32_t vm_flags,
            const char *name,
            void *data,
            uintptr_t *out_vaddr) {
        // Align the sizes to page boundaries.
        size = ALIGN_UP(size, PAGE_SIZE);
        vm_size = ALIGN_UP(vm_size, PAGE_SIZE);
    
        /* Boundary check: ensure mapping fits in the address space.
           This check uses the provided hint, but later we may adjust it if needed. */
        if (hint + size > space->max_addr) {
            panic("vmap_internal: mapping for %s (hint: 0x%llx, size: %zu) exceeds address space bounds (max: 0x%llx)",
                  name, hint, size, space->max_addr);
        }
    
        /* If VM_FIXED is not set, treat the hint as a suggestion and search for a free region */
        if (!(vm_flags & VM_FIXED)) {
            uintptr_t free_addr = find_free_region(space, size, hint);
            if (free_addr == 0) {
                panic("vmap_internal: no free region found for %s", name);
            }
            hint = free_addr;
        }
    
        /* Allocate and initialize the vm_mapping_t structure */
        vm_mapping_t *vm = kmallocz(sizeof(vm_mapping_t));
        if (!vm) {
            return -1;
        }
        vm->type = type;
        vm->flags = vm_flags;
        vm->address = hint;  // Use the (potentially updated) hint
        vm->size = size;
        vm->virt_size = vm_size;
        vm->name = strdup(name);
        if (!vm->name) {
            kfree(vm);
            return -1;
        }
    
        switch (type) {
            case VM_TYPE_RSVD:
                break;
            case VM_TYPE_PHYS:
                vm->vm_phys = (uintptr_t)data;
                break;
            case VM_TYPE_PAGE:
                vm->vm_pages = (struct page *)data;
                break;
            case VM_TYPE_FILE:
                panic("vmap_internal: VM_TYPE_FILE not supported");
                break;
            default:
                panic("vmap_internal: unknown mapping type");
        }
    
        /* Insert the new mapping into the address space’s list */
        LIST_ADD(&space->mappings, vm, list);
        space->num_mappings++;
    
        /* If VM_NOMAP is not set, perform the actual mapping via lower-level functions */
        if (!(vm_flags & VM_NOMAP)) {
            size_t page_count = size / PAGE_SIZE;
            if (type == VM_TYPE_PHYS) {
                early_map_entries(vm->address, vm->vm_phys, page_count, vm_flags);
            } else if (type == VM_TYPE_PAGE) {
                if (vm->vm_pages) {
                    uintptr_t phys = vm->vm_pages->address;
                    early_map_entries(vm->address, phys, page_count, vm_flags);
                }
            }
        }
    
        if (out_vaddr) {
            *out_vaddr = vm->address;
        }
        return 0;
    }

/* Public API implementations */

uintptr_t vmap_rsvd(uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
    int res = vmap_internal(cur_space, VM_TYPE_RSVD, hint, size, size, vm_flags, name, NULL, &hint);
    if (res < 0) {
        panic("vmap_rsvd failed for %s", name);
    }
    return hint;
}

uintptr_t vmap_phys(uintptr_t phys_addr, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
    int res = vmap_internal(cur_space, VM_TYPE_PHYS, hint, size, size, vm_flags, name, (void *)phys_addr, &hint);
    if (res < 0) {
        panic("vmap_phys failed for %s", name);
    }
    return hint;
}

uintptr_t vmap_pages(page_t *pages, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
    int res = vmap_internal(cur_space, VM_TYPE_PAGE, hint, size, size, vm_flags, name, (void *)pages, &hint);
    if (res < 0) {
        panic("vmap_pages failed for %s", name);
    }
    return hint;
}

uintptr_t vmap_anon(size_t vm_size, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
    /* For anonymous mappings, allocate physical pages from PMM */
    size_t page_count = size / PAGE_SIZE;
    uintptr_t phys_addr = pmm_early_alloc_pages(page_count);
    if (!phys_addr) {
        panic("vmap_anon: out of physical memory");
    }
    int res = vmap_internal(cur_space, VM_TYPE_PHYS, hint, size, vm_size, vm_flags, name, (void *)phys_addr, &hint);
    if (res < 0) {
        panic("vmap_anon failed for %s", name);
    }
    return hint;
}

void *vm_mmap(uintptr_t addr, size_t len, int prot, int flags, int fd, off_t off) {
    /* A simplified mmap: we only support anonymous mappings here */
    uint32_t vm_flags = VM_USER;
    if (prot & 0x1) vm_flags |= VM_READ;
    if (prot & 0x2) vm_flags |= VM_WRITE;
    if (prot & 0x4) vm_flags |= VM_EXEC;
    if (flags & VM_FIXED) vm_flags |= VM_FIXED;

    uintptr_t vaddr = vmap_anon(len, addr, len, vm_flags, "mmap_anon");
    if (!vaddr) {
        return (void *)-1;
    }
    return (void *)vaddr;
}

int vmap_protect(uintptr_t vaddr, size_t len, int prot) {
    /* Stub: in a full implementation, this function would update the
       protection bits in the page tables. For now, we simply print a message. */
    kprintf("vmap_protect: Changing protection at 0x%llx, len %zu to prot %d\n", vaddr, len, prot);
    return 0;
}

int vmap_free(uintptr_t vaddr, size_t len) {
    /* Locate the mapping by matching the base address and size.
       In a real implementation, you might need to support partial unmapping. */
    vm_mapping_t *vm = NULL;
    LIST_FOREACH(vm, &cur_space->mappings, list) {
        if (vm->address == vaddr && vm->size == len) {
            break;
        }
    }
    if (!vm) {
        kprintf("vmap_free: mapping not found at 0x%llx, len %zu\n", vaddr, len);
        return -1;
    }
    remove_vm_mapping(cur_space, vm);
    return 0;
}

/* Kernel dynamic allocation via vmalloc/vfree.
   vmalloc creates an anonymous mapping in the kernel space (with VM_MALLOC flag).
   vfree frees that mapping. */
void *vmalloc(size_t size, uint32_t vm_flags) {
    uintptr_t vaddr = vmap_anon(size, KERNEL_HEAP_VA, size, vm_flags | VM_MALLOC, "vmalloc");
    if (!vaddr) {
        panic("vmalloc failed");
    }
    return (void *)vaddr;
}

void vfree(void *ptr) {
    if (!ptr) return;
    uintptr_t vaddr = (uintptr_t)ptr;
    /* For simplicity, we assume the allocation was exactly one page.
       A complete implementation would track allocation sizes. */
    int res = vmap_free(vaddr, PAGE_SIZE);
    if (res < 0) {
        panic("vfree failed for ptr %p", ptr);
    }
}

/* Address space management */

// Create a new address space from a given virtual address range.
address_space_t *vm_new_space(uintptr_t min_addr, uintptr_t max_addr, uintptr_t page_table) {
    address_space_t *space = kmallocz(sizeof(address_space_t));
    if (!space) {
        panic("vm_new_space: allocation failed");
    }
    space->min_addr = min_addr;
    space->max_addr = max_addr;
    space->page_table = page_table;
    space->num_mappings = 0;
    LIST_INIT(&space->mappings);
    /* new_tree can be added here if using an interval tree. */
    return space;
}

address_space_t *vm_current_space(void) {
    return cur_space;
}

void vm_set_current_space(address_space_t *space) {
    cur_space = space;
}

extern uintptr_t kernel_reserved_start;
extern uintptr_t kernel_reserved_va_ptr;



/* vmem_init()
   Called during kernel initialization to set up the kernel and default user
   address spaces. It also sets the current space (for now, the kernel space). */
void vmem_init(void) {
      // the page tables are still pretty much the same as what the bootloader set up for us
  //
  //   0x0000000000000000 - +1Gi           | identity mapped
  //   +1GB - 0x00007FFFFFFFFFFF           | unmapped
  //       ...
  //   === kernel mappings ===
  //   0xFFFF800000000000 - +1Mi           | mapped 0-1Mi
  //   kernel_code_start - kernel_code_end | kernel code (rw)
  //   kernel_code_end - kernel_data_end   | kernel data (rw)
  //       ...
  //   0xFFFFFF8000400000 - +6Mi           | kernel heap (rw)
  //       ...
  //   0xFFFFFF8000C00000 - +rsvd size     | kernel reserved (--)
  //

    dvm("Initializing virtual memory management system...");
    /* Assume boot_info_v2->pml4_addr is already set by the bootloader/pgtable module */
    uintptr_t pgtable = get_current_pgtable();

    kprintf("kernel code start: %llx\n", __kernel_code_start);
    kprintf("kernel code end: %llx\n", __kernel_code_end);
    kprintf("kernel data end: %llx\n", __kernel_data_end);
    kprintf("kernel virtual offset: %llx\n", (uintptr_t)__kernel_virtual_offset);
    uintptr_t kernel_phys = (uintptr_t)__kernel_code_start - (uintptr_t)__kernel_virtual_offset;
    size_t kernel_size = (uintptr_t)__kernel_data_end - (uintptr_t)__kernel_code_start;
    uintptr_t kernel_vaddr = (uintptr_t)__kernel_virtual_offset + (uintptr_t)__kernel_code_start;
    size_t lowmem_size = (uintptr_t)__kernel_address;
    size_t kernel_code_size = (uintptr_t)__kernel_code_end - (uintptr_t)__kernel_code_start;
    size_t kernel_data_size = (uintptr_t)__kernel_data_end - (uintptr_t)__kernel_code_end;
    size_t reserved_size = kernel_reserved_va_ptr - KERNEL_RESERVED_VA;
    kprintf("kernel_phys: %llx\n", kernel_phys);
    kprintf("kernel_size: %llx\n", kernel_size);
    kprintf("kernel_vaddr: %llx\n", kernel_vaddr);

    kernel_space = vm_new_space(KERNEL_SPACE_START, KERNEL_SPACE_END, 0);
    default_user_space = vm_new_space(USER_SPACE_START, USER_SPACE_END, pgtable);
    vm_set_current_space(default_user_space);

    // initial address space layout
    uint32_t kvm_flags = VM_FIXED | VM_NOMAP | VM_MAPPED;
    // we are describing existing mappings, don't remap them
    vmap_rsvd(0, PAGE_SIZE, VM_USER | kvm_flags, "null");
    kprintf("boot_info_v2: %llx, __kernel_virtual_offset: %llx, lowmem: %llx\n", boot_info_v2, &__kernel_virtual_offset, lowmem_size);
    vmap_phys(0, (uintptr_t)(&__kernel_virtual_offset), lowmem_size, VM_RDWR | kvm_flags, "lowmem");
    vmap_phys((uintptr_t)__kernel_address, (uintptr_t)__kernel_code_start, kernel_code_size, VM_RDEXC | kvm_flags, "kernel code");
    vmap_phys((uintptr_t)__kernel_address + kernel_code_size, (uintptr_t)__kernel_code_end, kernel_data_size, VM_RDWR | kvm_flags, "kernel data");
    vmap_phys(kheap_phys_addr(), KERNEL_HEAP_VA, KERNEL_HEAP_SIZE, VM_RDWR | kvm_flags, "kernel heap");
    vmap_phys(kernel_reserved_start, KERNEL_RESERVED_VA, reserved_size, VM_RDWR | kvm_flags, "kernel reserved");
    //////////

    // remap boot info struct
    static_assert(sizeof(boot_info_v2) <= PAGE_SIZE);
    vm_print_address_space();
    boot_info_v2 = (void *)vmap_phys((uintptr_t)boot_info_v2, 0, PAGE_SIZE, VM_WRITE, "boot info");

    // fork the default address space but dont deepcopy the user page tables as as
    // to effectively "unmap" the user identity mappings in out new address space.
    // This leaves the original page tables (identity mappings included) for out APs
    // address_space_t *user_space = vm_fork_space(default_user_space, false); // deepcopt_user = false
    // set_current_pgtable(user_space->page_table);
    // set_curspace(user_space);
    // curproc->space = user_space;
    dvm("Virtual memory management system initialized.");
}

/* Debug: Print all mappings in the current address space */
void vm_print_address_space(void) {
    kprintf("Current address space mappings:\n");
    vm_mapping_t *vm;
    LIST_FOREACH(vm, &cur_space->mappings, list) {
        char flags_buf[128];
        uint16_t flags = vm_flags_to_pe_flags(vm->flags);
        flags_to_str_r(flags, flags_buf, sizeof(flags_buf));
        kprintf("  Mapping: %s @ 0x%llx-@%llx, size: %zu bytes, flags: %s | %llx\n",
                vm->name, vm->address, vm->address + vm->size, vm->size, flags_buf, vm->flags);
    }
}

uintptr_t get_default_ap_pml4() {
    return default_user_space->page_table;
}

/*
 * ioremap: Maps a physical I/O memory region into the kernel virtual address space.
 *
 * Parameters:
 *   phys_addr - the physical address of the I/O memory.
 *   size      - the size of the I/O region to map.
 *
 * Returns:
 *   A pointer to the virtual address that now maps the given physical address range.
 *
 * Note:
 *   This implementation uses the existing vmap_phys() function with VM_READ | VM_WRITE
 *   permissions. In a more complete implementation you might want to add special caching
 *   attributes (e.g., non-cacheable) by extending the VM flag definitions.
 */
void *ioremap(uintptr_t phys_addr, size_t size, char *name) {
    uintptr_t vaddr = vmap_phys(phys_addr, IOREMAP_BASE, size, VM_READ | VM_WRITE | VM_NOCACHE, name);
    return (void *)vaddr;
}

/*
 * Optionally, you can implement a matching iounmap function.
 *
 * iounmap: Unmaps a previously ioremap()-ed region.
 *
 * Parameters:
 *   addr - the virtual address that was returned by ioremap().
 *   size - the size of the mapped region.
 *
 * Note:
 *   This simply delegates to vmap_free() from your virtual memory manager.
 */
void iounmap(void *addr, size_t size) {
    if (!addr) {
        return;
    }
    int res = vmap_free((uintptr_t)addr, size);
    if (res < 0) {
        panic("iounmap failed for addr %p, size %zu", addr, size);
    }
}

/*
 * struct page_sirectory_entry
 *
 * @present: 
 * @read_write: if 0, writes are not allowed to subsequent entry
 * @user_super: if 0, user-mode access is not allowed to subsequent entry
 * @page_level_write_through: memory type
 * @page_level_cache_disabled: memory type
 * @accessed: indicated whether this page has been accessed
 * @ignore0:
 * @page_size: if 0, subsequent entry is a page_table
 * @ignore1:
 * @available: available for use by the kernel
 * @address:  physical address of the page referenced by this entry
*/

/*
struct page_directory_entry_t
{
        uint64_t value;
};

enum PT_FLAG
{
        PT_PRESENT = 0,
        PT_RW      = 1,
        PT_US      = 2,
        PT_PWT     = 3,
        PT_PCD     = 4,
        PT_A       = 5,

        PT_PS      = 7,

        PT_AVAIL1   = 9,
        PT_AVAIL2   = 10,
        PT_AVAIL3   = 11,
        PT_NX       = 63,
};

struct page_table
{
    struct page_directory_entry_t entries[512];
};

static struct page_table* PML4;

static void vmem_set_flag(struct page_directory_entry_t* entry, enum PT_FLAG flag, bool enabled);
static bool vmem_get_flag(struct page_directory_entry_t* entry, enum PT_FLAG flag);
static uint64_t vmem_get_address(struct page_directory_entry_t* entry);
static void vmem_set_address(struct page_directory_entry_t* entry, uint64_t address);

void vmem_memory_map(void* vAddr, void* pAddr)
{
        uint64_t p_idx   = ((uint64_t)vAddr >> 12) & 0x1ff;
        uint64_t pt_idx  = ((uint64_t)vAddr >> 21) & 0x1ff;
        uint64_t pd_idx  = ((uint64_t)vAddr >> 30) & 0x1ff;
        uint64_t pdp_idx = ((uint64_t)vAddr >> 39) & 0x1ff;

        struct page_directory_entry_t pde = PML4->entries[pdp_idx];
        struct page_table* pdp;

        if (!vmem_get_flag(&pde, PT_PRESENT))
        {
                pdp = (struct page_table*)bitmap_page_request();
                memset(pdp, 0, PAGE_SIZE);

                vmem_set_address(&pde, (uint64_t)pdp >> 12);
                vmem_set_flag(&pde, PT_PRESENT, true);
                vmem_set_flag(&pde, PT_RW, true);
                PML4->entries[pdp_idx] = pde;
        }
        else
                pdp = (struct page_table*)(vmem_get_address(&pde) << 12);

        pde = pdp->entries[pd_idx];
        struct page_table* pd;
        if (!vmem_get_flag(&pde, PT_PRESENT))
        {
                pd = (struct page_table*)bitmap_page_request();
                memset(pd, 0, PAGE_SIZE);

                vmem_set_address(&pde, (uint64_t)pd >> 12);
                vmem_set_flag(&pde, PT_PRESENT, true);
                vmem_set_flag(&pde, PT_RW, true);
                pdp->entries[pd_idx] = pde;
        }
        else
                pd = (struct page_table*)(vmem_get_address(&pde) << 12);

        pde = pd->entries[pt_idx];
        struct page_table* pt;
        if (!vmem_get_flag(&pde, PT_PRESENT))
        {
                pt = (struct page_table*)bitmap_page_request();
                memset(pt, 0, PAGE_SIZE);

                vmem_set_address(&pde, (uint64_t)pt >> 12);
                vmem_set_flag(&pde, PT_PRESENT, true);
                vmem_set_flag(&pde, PT_RW, true);
                pd->entries[pt_idx] = pde;
        }
        else
                pt = (struct page_table*)(vmem_get_address(&pde) << 12);

        pde = pt->entries[p_idx];
        vmem_set_address(&pde, (uint64_t)pAddr >> 12);
        vmem_set_flag(&pde, PT_PRESENT, true);
        vmem_set_flag(&pde, PT_RW, true);
        pt->entries[p_idx] = pde;
}

extern uint64_t total_memory;

void vmem_init(uint64_t fb_base, uint64_t fb_size)
{
        serial_port_write("vmem_init() #1");
        PML4 = (struct page_table*)bitmap_page_request();
        serial_port_write("vmem_init() #2");
        memset(PML4, 0, PAGE_SIZE);
        serial_port_write("vmem_init() #3");
        
        for (uint64_t idx = 0; idx < total_memory; idx += PAGE_SIZE)    // Identity map all pages
                vmem_memory_map((void*)idx, (void*)idx);

        serial_port_write("vmem_init() #4");
        
        uint64_t fbBase = fb_base;
        uint64_t fbSize = fb_size + 0x1000;
        pmm_pages_lock((void*)fbBase, fbSize/ 0x1000 + 1);

        uint64_t cr4;
        __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
        cr4 |= (1 << 5); // Set PAE
        __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

        uint64_t efer;
        __asm__ volatile("rdmsr" : "=A"(efer) : "c"(0xC0000080));
        efer |= (1 << 8); // Set LME
        __asm__ volatile("wrmsr" : : "c"(0xC0000080), "A"(efer));
        // drawRect(0, 0, 100, 200, 0xAAAAFFFF);
        uint64_t pml4_phys = (uint64_t)PML4 & 0xFFFFFFFFFFFFF000ULL;
        __asm__ volatile("cli");
        __asm__ __volatile__("movq %0, %%cr3" : : "r"(PML4) : "memory");
	__asm__ volatile("sti");
//        __asm__ volatile ("movq %0, %%cr3" : : "r" (pml4_phys));
        // drawRect(0, 0, 2000, 1000, 0xAAAA00FF);
}

static void vmem_set_flag(struct page_directory_entry_t* entry, enum PT_FLAG flag, bool enabled)
{
        if (enabled)
                entry->value |= (1 << flag);
        else
                entry->value &= ~(1 << flag);
}

static bool vmem_get_flag(struct page_directory_entry_t* entry, enum PT_FLAG flag)
{
        return (entry->value & (1 << flag)) ? true : false;
}

static uint64_t vmem_get_address(struct page_directory_entry_t* entry)
{
        return (entry->value & 0x000FFFFFFFFFF000) >> 12;
}

static void vmem_set_address(struct page_directory_entry_t* entry, uint64_t address)
{
        address &= 0x000000FFFFFFFFFF;
        entry->value &= 0xFFF0000000000FF;
        entry->value |= (address << 12);
}
        */
