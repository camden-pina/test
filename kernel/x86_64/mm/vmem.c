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
#include <errno.h>
#include <init.h>
#include <mm_types.h>

#include <queue.h>  // Include your queue definitions

/*
   This module implements a high-level virtual memory manager.
   It creates and maintains an address_space_t structure that holds
   a linked list of vm_mapping_t objects.
   
   Each mapping is created via an internal function vmap_internal() which,
   depending on the type (reserved, physical, pages, or file), uses lower-level
   mapping functions (like early_map_entries) to install the mapping into the page tables.
   
   This version implements full dynamic mapping features including:
     - A full choose_best_hint() function that selects default hints
     - Protection flag enforcement (if VM_WRITE/VM_EXEC, VM_READ is forced)
     - Dynamic virtual region calculation with VM_STACK support (including a guard page)
     - A free-region search that jumps past overlapping mappings
     - Proper error handling via panic() on fatal failures.
*/

#define do_align(x, al) ((al) > 0 ? (align(x, al)) : (x))

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
 * (These should be defined in a header or elsewhere in your kernel.)
 */
#ifndef KERNEL_HEAP_VA
#define KERNEL_HEAP_VA 0xFFFFFF8000400000ULL
#endif

#ifndef KERNEL_HEAP_SIZE
#define KERNEL_HEAP_SIZE (6 * 1024 * 1024)  // 6MB heap
#endif

/*
 * Default hint addresses for different mapping types.
 */
#define HINT_USER_DEFAULT   0x0000000050000000ULL
#define HINT_USER_MALLOC    0x0000050000000000ULL
#define HINT_USER_STACK     0x0000800000000000ULL
#define HINT_KERNEL_DEFAULT 0xFFFFC00000000000ULL
#define HINT_KERNEL_MALLOC  0xFFFFC01000000000ULL
#define HINT_KERNEL_STACK   0xFFFFFF8040000000ULL

/* The vm_type enum */
enum vm_type { VM_TYPE_RSVD, VM_TYPE_PHYS, VM_TYPE_PAGE, VM_TYPE_FILE, VM_MAX_TYPE };

/* 
 * Helper: If VM_WRITE or VM_EXEC is set, force VM_READ.
 */
static inline uint32_t enforce_read(uint32_t vm_flags) {
    if (vm_flags & (VM_WRITE | VM_EXEC))
        vm_flags |= VM_READ;
    return vm_flags;
}

/* 
 * Remove the given vm_mapping_t from the address space’s linked list
 * and free its allocated memory.
 */
static void remove_vm_mapping(address_space_t *space, vm_mapping_t *vm) {
    /* Remove vm from the linked list whose head is in space->mappings.
       The LIST_REMOVE macro expects the address of the list head. */
    LIST_REMOVE(&space->mappings, vm, list);
    space->num_mappings--;

    if (vm->name)
        kfree(vm->name);
    kfree(vm);
}

static void vm_fork_internal(vm_mapping_t *vm, vm_mapping_t *new_vm) {
    kprintf("[DEBUG] Entering vm_fork_internal: vm=%p, new_vm=%p, vm->type=%d\n", vm, new_vm, vm->type);
    bool shared = new_vm->flags & VM_SHARED;
    kprintf("[DEBUG] New mapping shared flag: %s\n", shared ? "true" : "false");

    switch (vm->type) {
      case VM_TYPE_RSVD:
          kprintf("[DEBUG] vm->type is VM_TYPE_RSVD. No further action required.\n");
          break;
      case VM_TYPE_PHYS:
          kprintf("[DEBUG] vm->type is VM_TYPE_PHYS. Copying physical mapping: %p\n", vm->vm_phys);
          new_vm->vm_phys = vm->vm_phys;
          break;
      case VM_TYPE_PAGE:
          kprintf("[DEBUG] vm->type is VM_TYPE_PAGE. Allocating copy-on-write pages from %p\n", vm->vm_pages);
          new_vm->vm_pages = alloc_cow_pages(vm->vm_pages);
          kprintf("[DEBUG] Allocated COW pages: %p\n", new_vm->vm_pages);
          break;
      case VM_TYPE_FILE:
          kprintf("[DEBUG] vm->type is VM_TYPE_FILE. File mapping fork unimplemented, invoking panic.\n");
          panic("unimplemented: new_vm->vm_file = vm_file_fork(vm->vm_file);");
          break;
      default:
          kprintf("[ERROR] vm_fork_internal: Invalid mapping type %d\n", vm->type);
          panic("vm_fork_internal: invalid mapping type");
    }

    kprintf("[DEBUG] Exiting vm_fork_internal. new_vm updated successfully.\n");
        for (;;);
}

static vm_mapping_t *vm_struct_alloc(enum vm_type type, uint32_t vm_flags, uintptr_t vaddr, size_t size, size_t virt_size) {
    kprintf("[DEBUG] Allocating vm_struct: type=%d, flags=0x%x, vaddr=0x%lx, size=%zu, virt_size=%zu\n", type, vm_flags, vaddr, size, virt_size);
    vm_mapping_t *vm = kmallocz(sizeof(vm_mapping_t));
    if (!vm) {
        kprintf("[ERROR] vm_struct_alloc: kmallocz failed to allocate memory for vm mapping.\n");
        return NULL;
    }
    vm->type = type;
    vm->flags = vm_flags;
    vm->address = vaddr;
    vm->size = size;
    vm->virt_size = virt_size;
    kprintf("[DEBUG] vm_struct_alloc: Successfully allocated vm mapping at %p\n", vm);
    return vm;
}

/*
 * vm_fork_space - Create a duplicate (fork) of an address space.
 */
address_space_t *vm_fork_space(address_space_t *space, bool deepcopy_user) {
    kprintf("[DEBUG] Starting vm_fork_space: space=%p, deepcopy_user=%d\n", space, deepcopy_user);
    kprintf("[DEBUG] Source space boundaries: min_addr=0x%lx, max_addr=0x%lx\n", space->min_addr, space->max_addr);

    /* Create a new address space with the same min/max boundaries */
    address_space_t *newspace = vm_new_space(space->min_addr, space->max_addr, 0);
    if (!newspace) {
        kprintf("[ERROR] vm_fork_space: vm_new_space failed to create a new address space.\n");
        panic("vm_fork_space: failed to create new address space");
    }
    kprintf("[DEBUG] New address space created: %p\n", newspace);

    /* Verify that we are forking the current page table */
    kprintf("[DEBUG] Verifying page table: space->page_table=%p, current_pgtable=%p\n", (void*)space->page_table, (void*)get_current_pgtable());
    kassert(space->page_table == get_current_pgtable());

    /* Fork the page tables */
    kprintf("[DEBUG] Forking page tables...\n");
    page_t *meta_pages = NULL;
    uintptr_t new_pgtable = fork_page_tables(&meta_pages, deepcopy_user);
    kprintf("[DEBUG] Forked page tables: new_pgtable=0x%lx, meta_pages=%p\n", new_pgtable, meta_pages);

    /* Update the new address space with the forked page table */
    newspace->page_table = new_pgtable;
    kprintf("[DEBUG] Updated newspace->page_table with forked table.\n");

    /* Add the meta pages to the new address space's list */
    kprintf("[DEBUG] Adding meta pages to newspace->table_pages...\n");
    SLIST_ADD_SLIST(&newspace->table_pages, meta_pages, SLIST_GET_LAST(meta_pages, next), next);

    /* Initialize the mapping count */
    newspace->num_mappings = 0;
    vm_mapping_t *prev_newvm = NULL;
    vm_mapping_t *vm;

    /* Iterate over each mapping in the source address space */
    LIST_FOREACH(vm, &space->mappings, list) {
        kprintf("[DEBUG] Processing mapping: name=%s, vm=%p, type=%d\n", vm->name, vm, vm->type);

        /* Allocate a new mapping structure for the forked mapping */
        vm_mapping_t *newvm = vm_struct_alloc(vm->type, vm->flags, vm->address, vm->size, vm->virt_size);
        if (!newvm) {
            kprintf("[ERROR] vm_fork_space: Failed to allocate mapping for %s\n", vm->name);
            panic("vm_fork_space: failed to allocate mapping for %s", vm->name);
        }
        /* Duplicate the mapping name and update the space pointer */
        newvm->name  = strdup(vm->name);
        if (!newvm->name) {
            kprintf("[ERROR] vm_fork_space: strdup failed for mapping name: %s\n", vm->name);
            panic("vm_fork_space: failed to duplicate mapping name for %s", vm->name);
        }
        newvm->space = newspace;

        kprintf("[DEBUG] Forking internal mapping for %s (newvm=%p)...\n", vm->name, newvm);
        vm_fork_internal(vm, newvm);

        /*
         * Instead of inserting into an interval tree, add the new mapping to the new address space's linked list.
         */
        if (prev_newvm) {
            kprintf("[DEBUG] Inserting new mapping %p after previous mapping %p\n", newvm, prev_newvm);
            LIST_INSERT(&newspace->mappings, newvm, list, prev_newvm);
        } else {
            kprintf("[DEBUG] Adding first new mapping %p to newspace->mappings\n", newvm);
            LIST_ADD(&newspace->mappings, newvm, list);
        }
        prev_newvm = newvm;

        /* Update the mapping count */
        newspace->num_mappings++;
        kprintf("[DEBUG] Mapping count updated to %d\n", newspace->num_mappings);
    }

    kprintf("[DEBUG] vm_fork_space completed: newspace=%p with %d mappings\n", newspace, newspace->num_mappings);
    return newspace;
}

/*
 * Full-featured choose_best_hint.
 * If the provided hint is within the proper range, use it;
 * otherwise return a default based on whether the mapping is for user/kernel,
 * stack, or malloc.
 */
static uintptr_t choose_best_hint(uintptr_t hint, uint32_t vm_flags) {
    if (vm_flags & VM_USER) {
        if (hint > 0 && hint < USER_SPACE_END) {
          return hint;
        }
    
        if (vm_flags & VM_STACK)
          return HINT_USER_STACK;
        if (vm_flags & VM_MALLOC)
          return HINT_USER_MALLOC;
        return HINT_USER_DEFAULT;
      } else {
        if (hint > KERNEL_SPACE_START && hint < KERNEL_SPACE_END) {
          return hint;
        }
    
        if (vm_flags & VM_STACK)
          return HINT_KERNEL_STACK;
        if (vm_flags & VM_MALLOC)
          return HINT_KERNEL_MALLOC;
        return HINT_KERNEL_DEFAULT;
      }
}

/*
 * Free Region Search (Linked-list based)
 */

/* Check if the region [start, start+size) is free in the given address space */
static int is_region_free(address_space_t *space, uintptr_t start, size_t size) {
    vm_mapping_t *vm;
    LIST_FOREACH(vm, &space->mappings, list) {
        // Check if candidate region overlaps mapping [vm->address, vm->address+vm->size)
        if (!(start + size <= vm->address || start >= vm->address + vm->size))
            return 0;
    }
    return 1;
}

/*
 * Find a free region of at least 'size' bytes starting at or after 'hint'.
 * This version jumps candidate forward to the end of any overlapping mapping.
 */
static uintptr_t find_free_region(address_space_t *space, size_t size, uintptr_t hint) {
    uintptr_t candidate = (hint < space->min_addr) ? space->min_addr : hint;
    while (candidate + size <= space->max_addr) {
        kprintf("FD");
        if (is_region_free(space, candidate, size))
            return candidate;
        vm_mapping_t *vm;
        int advanced = 0;
        LIST_FOREACH(vm, &space->mappings, list) {
            if (!(candidate + size <= vm->address || candidate >= vm->address + vm->size)) {
                if (vm->address + vm->size > candidate) {
                    candidate = vm->address + vm->size;
                    advanced = 1;
                }
            }
        }
        if (!advanced)
            candidate += PAGE_SIZE;
    }
    return 0; // No suitable region found
}

/* Return the maximum of two uintptr_t values */
static inline uintptr_t max_uintptr(uintptr_t a, uintptr_t b) {
    return (a > b) ? a : b;
}

static address_space_t *select_space(address_space_t *user_space, uintptr_t addr) {
    if (addr >= KERNEL_SPACE_START) {
      return kernel_space;
    }
    return user_space;
  }

/* Check if the region [base, base+size) is free in the given address space.
 * If not free, *closest_vm is set to the mapping that overlaps or immediately
 * follows the candidate range.
 */
static bool check_range_free(
    address_space_t *space,
    uintptr_t base,
    size_t size,
    uint32_t vm_flags,
    vm_mapping_t **closest_vm
) {
    dvm("check_range_free: Checking region [0x%llx, 0x%llx)", base, base + size);

    if (!is_region_free(space, base, size)) {
        dvm("check_range_free: Region is NOT free");
        vm_mapping_t *closest = NULL;
        vm_mapping_t *vm;
        LIST_FOREACH(vm, &space->mappings, list) {
            dvm("check_range_free: Inspecting mapping '%s' at [0x%llx, 0x%llx)", 
                vm->name, vm->address, vm->address + vm->size);
            if (vm->address + vm->size > base) {
                if (!closest || vm->address < closest->address) {
                    closest = vm;
                    dvm("check_range_free: Found closer mapping '%s' at [0x%llx, 0x%llx)",
                        vm->name, vm->address, vm->address + vm->size);
                }
            }
        }
        if (closest)
            dvm("check_range_free: Returning closest mapping '%s' at [0x%llx, 0x%llx)",
                closest->name, closest->address, closest->address + closest->size);
        else
            dvm("check_range_free: No mapping found overlapping or after the region");

        if (closest_vm)
            *closest_vm = closest;
        return false;
    }
    
    dvm("check_range_free: Region is free");
    /* Even if free, determine if any mapping immediately follows this region */
    vm_mapping_t *closest = NULL;
    vm_mapping_t *vm;
    LIST_FOREACH(vm, &space->mappings, list) {
        if (vm->address >= base) {
            if (!closest || vm->address < closest->address) {
                closest = vm;
                dvm("check_range_free: Candidate closest mapping updated to '%s' at [0x%llx, 0x%llx)",
                    vm->name, vm->address, vm->address + vm->size);
            }
        }
    }
    if (!closest)
        dvm("check_range_free: No mapping found after candidate free region");
    else
        dvm("check_range_free: Closest mapping after free region is '%s' at [0x%llx, 0x%llx)",
            closest->name, closest->address, closest->address + closest->size);
    
    if (closest_vm)
        *closest_vm = closest;
    
    return true;
}

/* Find a free region of at least 'size' bytes (aligned to 'align') starting at or after 'base'.
 * If a mapping overlaps the candidate region, *closest_vm is set to that mapping.
 */
static uintptr_t get_free_region(
    address_space_t *space,
    uintptr_t base,
    size_t size,
    uintptr_t align,
    uint32_t vm_flags,
    vm_mapping_t **closest_vm
) {
    base = do_align(base, align);
    size = do_align(size, align);
    dvm("get_free_region: Starting with base=0x%llx, size=0x%llx, align=0x%llx", base, size, align);

    if (size > (UINT64_MAX - base) || base + size > space->max_addr) {
        panic("get_free_region: no free address space: base=0x%llx, size=0x%llx, space->max_addr=0x%llx", base, size, space->max_addr);
    }

    uintptr_t candidate = (base < space->min_addr) ? space->min_addr : base;
    dvm("get_free_region: Initial candidate set to 0x%llx", candidate);
    
    while (candidate + size <= space->max_addr) {
        dvm("get_free_region: Checking candidate region [0x%llx, 0x%llx)", candidate, candidate + size);
        if (is_region_free(space, candidate, size)) {
            dvm("get_free_region: Candidate region [0x%llx, 0x%llx) is free", candidate, candidate + size);
            /* Find the mapping (if any) immediately after the free region */
            vm_mapping_t *closest = NULL;
            vm_mapping_t *vm;
            LIST_FOREACH(vm, &space->mappings, list) {
                if (vm->address >= candidate) {
                    if (!closest || vm->address < closest->address)
                        closest = vm;
                }
            }
            if (closest) {
                dvm("get_free_region: Closest mapping after free region is '%s' at [0x%llx, 0x%llx)",
                    closest->name, closest->address, closest->address + closest->size);
            } else {
                dvm("get_free_region: No mapping found immediately after candidate free region");
            }
            if (closest_vm)
                *closest_vm = closest;
            return candidate;
        }
        
        dvm("get_free_region: Candidate region [0x%llx, 0x%llx) is NOT free", candidate, candidate + size);
        uintptr_t new_candidate = candidate;
        vm_mapping_t *vm;
        LIST_FOREACH(vm, &space->mappings, list) {
            if (!(candidate + size <= vm->address || candidate >= vm->address + vm->size)) {
                dvm("get_free_region: Overlap detected with mapping '%s' at [0x%llx, 0x%llx)",
                    vm->name, vm->address, vm->address + vm->size);
                if (vm->address + vm->size > new_candidate) {
                    new_candidate = vm->address + vm->size;
                    dvm("get_free_region: Advancing candidate to 0x%llx", new_candidate);
                }
            }
        }
        if (new_candidate == candidate) {
            candidate += PAGE_SIZE;
            dvm("get_free_region: No overlapping mapping found, advancing candidate by PAGE_SIZE to 0x%llx", candidate);
        } else {
            candidate = new_candidate;
            dvm("get_free_region: Candidate updated to 0x%llx", candidate);
        }
        candidate = do_align(candidate, align);
        dvm("get_free_region: Candidate aligned to 0x%llx", candidate);
    }
    
    dvm("get_free_region: Exhausted address space; no free region found");
    if (closest_vm)
        *closest_vm = NULL;
    return 0; // No suitable free region found.
}

/*
 * vmap_internal: Updated mapping function with full features.
 *
 * - Aligns size and vm_size to PAGE_SIZE.
 * - Enforces that if VM_WRITE or VM_EXEC are requested, VM_READ is added.
 * - Computes the effective virtual region size as the max(vm_size, size).
 * - For VM_STACK mappings, adds a guard page and adjusts the offset.
 * - If VM_FIXED is set, uses the provided hint (with adjustments for stacks);
 *   otherwise, uses choose_best_hint() and finds a free region.
 * - Checks boundaries against the address space.
 * - Allocates and initializes a vm_mapping_t structure and inserts it into the space’s list.
 * - If VM_NOMAP is not set, performs the actual mapping (via early_map_entries).
 * - Returns the final virtual address via out_vaddr.
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
                            kprintf("\n\nHINT: %llx\n\n\n", hint);
    /* Align sizes */
    size = ALIGN_UP(size, PAGE_SIZE);
    vm_size = ALIGN_UP(vm_size, PAGE_SIZE);

    /* Enforce VM_READ if needed */
    vm_flags = enforce_read(vm_flags);

    /* Determine the effective page size */
    size_t pg_size = vm_flags_to_size(vm_flags);

    /* Compute the effective virtual region size (at least large enough to hold the mapping) */
    size_t virt_size = (vm_size > size) ? vm_size : size;
    size_t virt_off = 0;
    uintptr_t virt_base = 0;

    if (vm_flags & VM_FIXED) {
        kprintf("FIXED");
        if (vm_flags & VM_STACK) {
            /* For fixed stack mappings, add a guard page at the bottom */
            virt_size += PAGE_SIZE;
            virt_off = virt_size - size;
            if (hint < virt_off) {
                kprintf("hint < virt_off\n");
                return -EINVAL;
            }
            virt_base = hint - virt_off;
        } else {
            virt_off = 0;
            virt_base = hint;
        }
    } else {
        /* Dynamic mapping: choose a best hint and search for a free region */
        kprintf("hint1: %llx\n", hint);
        hint = choose_best_hint(hint, vm_flags);
        kprintf("hint: %llx\n", hint);
        if (vm_flags & VM_STACK) {
            virt_size += PAGE_SIZE;  /* guard page */
            virt_off = PAGE_SIZE;
            virt_base = max_uintptr(hint, virt_size);
        } else {
            virt_off = 0;
            virt_base = hint;
        }
    }

    address_space_t *_space = select_space(space, virt_base);

    int res = 0;
    vm_mapping_t *closest = NULL;
    if (vm_flags & VM_FIXED) {
        // make sure the requested range is free
        if (!check_range_free(_space, virt_base, virt_size, vm_flags, &closest)) {
            panic("requested fixed address range is not free %llx-%llx [name=%s]", virt_base, virt_base+virt_size, name);
            res = -EADDRNOTAVAIL;
        }
    } else {
        // dynamically allocated (use virt_base as starting point)
        virt_base = get_free_region(_space, virt_base, virt_size, pg_size, vm_flags, &closest);
        if (virt_base == 0) {
            panic("Failed to satisfy allocation request [name=%s]", name);
            res = -ENOMEM;
            return res;
        }
    }
        /*
        uintptr_t free_addr = find_free_region(space, virt_size, virt_base);
        if (free_addr == 0) {
            kprintf("Failed to find free region with virt_size: %llx, at virt_base: %llx, with hint: %llx\n", virt_size, virt_base, hint);
            return -ENOMEM;
        }
        virt_base = free_addr;
        */

    vm_mapping_t *vm = kmallocz(sizeof(vm_mapping_t));
    if (!vm) {
        kprintf("Failed to allocate memory with kmallosz\n");
        return -ENOMEM;
    }
    vm->type = type;
    vm->flags = vm_flags;
    /* The final virtual address is computed as base + offset */
    vm->address = virt_base + virt_off;
    vm->size = size;
    vm->virt_size = virt_size;
    vm->name = strdup(name);
    if (!vm->name) {
        kprintf("!vm->name\n");
        kfree(vm);
        return -ENOMEM;
    }

    switch (type) {
        case VM_TYPE_RSVD:
            break;
        case VM_TYPE_PHYS:
            vm->vm_phys = (uintptr_t)data;
            break;
        case VM_TYPE_PAGE:
            vm->vm_pages = (page_t *)data;
            break;
        case VM_TYPE_FILE:
            panic("vmap_internal: VM_TYPE_FILE not supported in this implementation");
            break;
        default:
            panic("vmap_internal: unknown mapping type");
    }

    /* Insert the new mapping into the address space’s list */
    LIST_ADD(&_space->mappings, vm, list);
    _space->num_mappings++;

    /* If VM_NOMAP is not set, perform the actual mapping */
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
    } else {
        vm->flags ^= VM_NOMAP;  /* clear the flag */
        vm->flags |= VM_MAPPED;
    }

    if (out_vaddr)
        *out_vaddr = virt_base + virt_off;
    return 0;
}

/* Public API implementations */

uintptr_t vmap_rsvd(uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
    uintptr_t vaddr;
    int res = vmap_internal(cur_space, VM_TYPE_RSVD, hint, size, size, vm_flags, name, NULL, &vaddr);
    if (res < 0)
        panic("vmap_rsvd failed for %s", name);
    return vaddr;
}

uintptr_t vmap_phys(uintptr_t phys_addr, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
    uintptr_t vaddr;
    int res = vmap_internal(cur_space, VM_TYPE_PHYS, hint, size, size, vm_flags, name, (void *)phys_addr, &vaddr);
    if (res < 0)
        panic("vmap_phys failed for %s, error_code: %d", name);
    return vaddr;
}

uintptr_t vmap_pages(page_t *pages, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
    uintptr_t vaddr;
    int res = vmap_internal(cur_space, VM_TYPE_PAGE, hint, size, size, vm_flags, name, (void *)pages, &vaddr);
    if (res < 0)
        panic("vmap_pages failed for %s", name);
    return vaddr;
}

uintptr_t vmap_anon(size_t vm_size, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
    size_t page_count = size / PAGE_SIZE;
    uintptr_t phys_addr = pmm_early_alloc_pages(page_count);
    if (!phys_addr)
        panic("vmap_anon: out of physical memory");
    uintptr_t vaddr;
    int res = vmap_internal(cur_space, VM_TYPE_PHYS, hint, size, vm_size, vm_flags, name, (void *)phys_addr, &vaddr);
    if (res < 0)
        panic("vmap_anon failed for %s", name);
    return vaddr;
}

/* A simplified mmap that supports anonymous mappings only */
void *vm_mmap(uintptr_t addr, size_t len, int prot, int flags, int fd, off_t off) {
    uint32_t vm_flags = VM_USER;
    if (prot & 0x1) vm_flags |= VM_READ;
    if (prot & 0x2) vm_flags |= VM_WRITE;
    if (prot & 0x4) vm_flags |= VM_EXEC;
    if (flags & VM_FIXED) vm_flags |= VM_FIXED;
    uintptr_t vaddr = vmap_anon(len, addr, len, vm_flags, "mmap anon");
    if (!vaddr)
        return (void *)-1;
    return (void *)vaddr;
}

/* vmap_protect: Stub that prints a message (full implementation would update page tables) */
int vmap_protect(uintptr_t vaddr, size_t len, int prot) {
    kprintf("vmap_protect: Changing protection at 0x%llx, len %zu to prot %d\n", vaddr, len, prot);
    return 0;
}

/* vmap_free: Remove the mapping that exactly matches the given vaddr and len */
int vmap_free(uintptr_t vaddr, size_t len) {
    vm_mapping_t *vm = NULL;
    LIST_FOREACH(vm, &cur_space->mappings, list) {
        if (vm->address == vaddr && vm->size == len)
            break;
    }
    if (!vm) {
        kprintf("vmap_free: mapping not found at 0x%llx, len %zu\n", vaddr, len);
        return -1;
    }
    /* Remove and free the mapping */
    remove_vm_mapping(cur_space, vm);
    return 0;
}

/* Kernel dynamic allocation via vmalloc/vfree.
   vmalloc creates an anonymous mapping in the kernel space (with VM_MALLOC flag).
   vfree frees that mapping.
*/
void *vmalloc(size_t size, uint32_t vm_flags) {
    uintptr_t vaddr = vmap_anon(size, KERNEL_HEAP_VA, size, vm_flags | VM_MALLOC, "vmalloc");
    if (!vaddr)
        panic("vmalloc failed");
    return (void *)vaddr;
}

void vfree(void *ptr) {
    if (!ptr)
        return;
    uintptr_t vaddr = (uintptr_t)ptr;
    /* For simplicity, assume the allocation was one page (a complete implementation would track sizes) */
    int res = vmap_free(vaddr, PAGE_SIZE);
    if (res < 0)
        panic("vfree failed for ptr %p", ptr);
}

/* Address space management */

address_space_t *vm_new_space(uintptr_t min_addr, uintptr_t max_addr, uintptr_t page_table) {
    address_space_t *space = kmallocz(sizeof(address_space_t));
    if (!space)
        panic("vm_new_space: allocation failed");
    space->min_addr = min_addr;
    space->max_addr = max_addr;
    space->page_table = page_table;
    space->num_mappings = 0;
    LIST_INIT(&space->mappings);
    return space;
}

address_space_t *vm_current_space(void) {
    return cur_space;
}

void vm_set_current_space(address_space_t *space) {
    cur_space = space;
}

/* External symbols assumed to be defined elsewhere */
extern uintptr_t kernel_reserved_start;
extern uintptr_t kernel_reserved_va_ptr;

/*
 * init_default_mappings()
 *
 * This function creates default mappings such as:
 *  - A reserved null page.
 *  - Fixed mappings for kernel code, data, heap, and reserved regions.
 *  - Remapping of the boot info structure.
 *
 * (The actual addresses are derived from linker symbols.)
 */
void init_default_mappings(void) {
    /* Implementation-specific – add your default mappings here */
}

/*
 * vmem_init()
 *
 * Called during kernel initialization to set up the kernel and default user
 * address spaces. Also sets the current space.
 */
void vmem_init(void) {
    dvm("Initializing virtual memory management system...");

    init_recursive_pgtable();

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
    
    uint32_t kvm_flags = VM_FIXED | VM_NOMAP | VM_MAPPED;
    /* Reserved null mapping */
    vmap_rsvd(0, PAGE_SIZE, VM_USER | kvm_flags, "null");
    kprintf("boot_info_v2: %llx, __kernel_virtual_offset: %llx, lowmem: %llx\n", boot_info_v2, &__kernel_virtual_offset, lowmem_size);
    /* Map low memory, kernel code, data, heap, and reserved regions */
    vmap_phys(0, (uintptr_t)(&__kernel_virtual_offset), lowmem_size, VM_RDWR | kvm_flags, "lowmem");
    vmap_phys((uintptr_t)__kernel_address, (uintptr_t)__kernel_code_start, kernel_code_size, VM_RDEXC | kvm_flags, "kernel code");
    vmap_phys((uintptr_t)__kernel_address + kernel_code_size, (uintptr_t)__kernel_code_end, kernel_data_size, VM_RDWR | kvm_flags, "kernel data");
    vmap_phys(kheap_phys_addr(), KERNEL_HEAP_VA, KERNEL_HEAP_SIZE, VM_RDWR | kvm_flags, "kernel heap");
    vmap_phys(kernel_reserved_start, KERNEL_RESERVED_VA, reserved_size, VM_RDWR | kvm_flags, "kernel reserved");
    
    execute_init_address_space_callbacks();
    
    /* Remap boot info struct */
    static_assert(sizeof(boot_info_v2) <= PAGE_SIZE);
    boot_info_v2 = (void *)vmap_phys((uintptr_t)boot_info_v2, 0, PAGE_SIZE, VM_WRITE, "boot info");

    vm_print_address_space();


    // fork the default address space but
    address_space_t *user_space = vm_new_space(USER_SPACE_START, USER_SPACE_END, pgtable);
    vm_set_current_space(user_space);

    // You can do user-space mappings here if you want, but do NOT redo the kernel mappings
    // Also, you might want a null page if you like:
    vmap_rsvd(0, PAGE_SIZE, VM_USER | VM_FIXED | VM_NOMAP | VM_MAPPED, "null");
    
    // DO NOT REMAP lowmem, kernel code, kernel data, heap, reserved, etc. again.
    // The kernel portion is globally shared. If you want to replicate it, you
    // typically copy or share the kernel portion of the page table, but do not
    // call vmap_phys again for the same addresses.

    // Now set the new CR3
    set_current_pgtable(user_space->page_table);

    vm_set_current_space(user_space);
    
    dvm("Virtual memory management system initialized.");
}

void init_ap_address_space() {
    address_space_t *user_space = vm_new_space(USER_SPACE_START, USER_SPACE_END, get_current_pgtable());
    vm_set_current_space(user_space);

    // You can do user-space mappings here if you want, but do NOT redo the kernel mappings
    // Also, you might want a null page if you like:
    vmap_rsvd(0, PAGE_SIZE, VM_USER | VM_FIXED | VM_NOMAP | VM_MAPPED, "null");
}

/* Debug: Print all mappings in the current address space */
void vm_print_address_space(void) {
    kprintf("Current address space mappings:\n");
    vm_mapping_t *vm;
    LIST_FOREACH(vm, &cur_space->mappings, list) {
        char flags_buf[128];
        uint16_t flags = vm_flags_to_pe_flags(vm->flags);  // assume conversion function exists
        flags_to_str_r(flags, flags_buf, sizeof(flags_buf)); // assume conversion function exists
        kprintf("  Mapping: %s @ 0x%llx-0x%llx, size: %zu bytes, flags: %s | 0x%llx\n",
                vm->name, vm->address, vm->address + vm->size, vm->size, flags_buf, vm->flags);
    }
}

uintptr_t get_default_ap_pml4() {
    return default_user_space->page_table;
}

/*
 * ioremap: Maps a physical I/O memory region into the kernel virtual address space.
 */
void *ioremap(uintptr_t phys_addr, size_t size, char *name) {
    uintptr_t vaddr = vmap_phys(phys_addr, IOREMAP_BASE, size, VM_READ | VM_WRITE | VM_NOCACHE, name);
    return (void *)vaddr;
}

/*
 * iounmap: Unmaps a previously ioremap()-ed region.
 */
void iounmap(void *addr, size_t size) {
    if (!addr)
        return;
    int res = vmap_free((uintptr_t)addr, size);
    if (res < 0)
        panic("iounmap failed for addr %p, size %zu", addr, size);
}
