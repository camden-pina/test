/*
#include <mm/vmalloc.h>
#include <mm/file.h>
#include <stdint.h>
#include <stddef.h>
#include <panic.h>
#include <errno.h>
#include <interval_tree.h>
#include <mm/pmm.h>
#include <mm/pgtable.h>

#define ASSERT(x) kassert(x)
#define DPRINTF(x, ...) kprintf(x, ##__VA_ARGS__)
#define EPRINTF(fmt, ...) kprintf("vmalloc: %s: " fmt, __func__, ##__VA_ARGS__)
#define DPANICF(x, ...) panic(x, ##__VA_ARGS__)
#define ALLOC_ERROR(msg, ...) panic(msg, ##__VA_ARGS__)

#define do_align(x, al) ((al) > 0 ? (align(x, al)) : (x))

#define T_ENTRY 509ULL // temp pdpt entry index
#define R_ENTRY 510ULL // recursive entry index

// these are the default hints for different combinations of vm flags
// they are used as a starting point for the kernel when searching for
// a free region
#define HINT_USER_DEFAULT   0x0000000050000000ULL // for VM_USER
#define HINT_USER_MALLOC    0x0000050000000000ULL // for VM_USER|VM_MALLOC
#define HINT_USER_STACK     0x0000800000000000ULL // for VM_USER|VM_STACK
#define HINT_KERNEL_DEFAULT 0xFFFFC00000000000ULL // for no flags
#define HINT_KERNEL_MALLOC  0xFFFFC01000000000ULL // for VM_MALLOC
#define HINT_KERNEL_STACK   0xFFFFFF8040000000ULL // for VM_STACK

extern uintptr_t entry_initial_stack_top;
address_space_t *default_user_space;
address_space_t *kernel_space;

// pdpe page for the temp entry
page_t *temp_pdpt_page;

// phys type

static void phys_type_map_internal(vm_mapping_t *vm, uintptr_t phys, size_t size, size_t off) {
  size_t stride = vm_flags_to_size(vm->flags);
  ASSERT(off % stride == 0);
  ASSERT(off + size <= vm->size);

  size_t count = size / stride;
  uintptr_t ptr = vm->address + off;
  uintptr_t phys_ptr = phys + off;
  while (count > 0) {
    page_t *table_pages = NULL;
    recursive_map_entry(ptr, phys_ptr, vm->flags, &table_pages);
    ptr += stride;
    phys_ptr += stride;
    count--;

    if (table_pages != NULL) {
      page_t *last_page = SLIST_GET_LAST(table_pages, next);
      SLIST_ADD_SLIST(&vm->space->table_pages, table_pages, last_page, next);
    }
  }

  cpu_flush_tlb();
}

static void phys_type_unmap_internal(vm_mapping_t *vm, size_t size, size_t off) {
  size_t stride = vm_flags_to_size(vm->flags);
  ASSERT(off % stride == 0);
  ASSERT(off + size <= vm->size);

  size_t count = size / stride;
  uintptr_t ptr = vm->address + off;
  while (count > 0) {
    recursive_unmap_entry(ptr, vm->flags);
    ptr += stride;
    count--;
  }

  cpu_flush_tlb();
}

// pages type

// static __ref page_t *page_type_fork_internal(page_t *pages, bool shared) {
//   if (!shared) {
//     return alloc_cow_pages(getref(pages));
//   }
// }

static void page_type_map_internal(vm_mapping_t *vm, page_t *pages, size_t size, size_t off) {
  size_t stride = vm_flags_to_size(vm->flags);
  ASSERT(off % stride == 0);
  ASSERT(off + size <= vm->size);

  size_t count = size / stride;
  uintptr_t ptr = vm->address + off;
  page_t *curr = pages;
  while (curr != NULL) {
    if (count == 0) {
      break;
    }

    struct pte *pte = page_get_mapping(curr, vm);
    if (pte != NULL) {
      // update existing mapping
      pgtable_update_entry_flags(ptr, pte->entry, vm->flags);
    } else {
      // create new mapping
      page_t *table_pages = NULL;
      uint64_t *entry = recursive_map_entry(ptr, curr->address, vm->flags, &table_pages);
      if (table_pages != NULL) {
        page_t *last_page = SLIST_GET_LAST(table_pages, next);
        SLIST_ADD_SLIST(&vm->space->table_pages, table_pages, last_page, next);
      }

      pte = pte_struct_alloc(curr, entry, vm);
      page_add_mapping(curr, pte);
    }

    ptr += stride;
    curr = curr->next;
    count--;
  }

  if (count > 0) {
    panic("not enough pages to map region {:str}\n", &vm->name);
  }
}

static void page_type_unmap_internal(vm_mapping_t *vm, size_t size, size_t off) {
  size_t stride = vm_flags_to_size(vm->flags);
  ASSERT(off % stride == 0);
  ASSERT(off + size <= vm->size);

  uintptr_t ptr = vm->address;
  page_t *curr = vm->vm_pages;
  // get to page at offset
  while (off > 0) {
    if (curr == NULL) {
      panic("page_type_unmap_internal: something went wrong");
    }
    curr = curr->next;
    ptr += stride;
    off -= stride;
  }

  uintptr_t max_ptr = ptr + size;
  while (ptr < max_ptr && curr != NULL) {
    struct pte *pte = page_remove_mapping(curr, vm);
    if (pte != NULL) {
      pgtable_update_entry_flags(ptr, pte->entry, 0);
      pte_struct_free(&pte);
    }
    ptr += stride;
    curr = curr->next;
  }
}

static void file_type_map_internal(vm_mapping_t *vm, size_t size, size_t off) {
  struct file_cb_data data = {vm, off, false};
  vm_file_visit_pages(vm->vm_file, off, vm->size, file_map_update_cb, &data);
}

static void file_type_unmap_internal(vm_mapping_t *vm, size_t size, size_t off) {
  struct file_cb_data data = {vm, off, true};
  vm_file_visit_pages(vm->vm_file, off, vm->size, file_map_update_cb, &data);
}

static void vm_update_internal(vm_mapping_t *vm, uint32_t prot) {
//  space_lock_assert(vm->space, MA_OWNED);
  prot &= VM_PROT_MASK;

  vm->flags &= ~VM_PROT_MASK;
  vm->flags |= prot;
  if (prot != 0) {
    vm->flags |= VM_MAPPED;
    switch (vm->type) {
      case VM_TYPE_PHYS:
        phys_type_map_internal(vm, vm->vm_phys, vm->size, 0);
        break;
      case VM_TYPE_PAGE:
        page_type_map_internal(vm, vm->vm_pages, vm->size, 0);
        break;
      case VM_TYPE_FILE:
        file_type_map_internal(vm, vm->size, 0);
        break;
      default:
        panic("vm_update_internal: invalid mapping type");
    }
  } else {
    vm->flags &= ~VM_MAPPED;
    switch (vm->type) {
      case VM_TYPE_PHYS:
        phys_type_unmap_internal(vm, vm->size, 0);
        break;
      case VM_TYPE_PAGE:
        page_type_unmap_internal(vm, vm->size, 0);
        break;
      case VM_TYPE_FILE:
        file_type_unmap_internal(vm, vm->size, 0);
        break;
      default:
        panic("vm_update_internal: invalid mapping type");
    }
  }
}

static vm_mapping_t *vm_struct_alloc(enum vm_type type, uint32_t vm_flags, uintptr_t vaddr, size_t size, size_t virt_size) {
  vm_mapping_t *vm = kmallocz(sizeof(vm_mapping_t));
  vm->type = type;
  vm->flags = vm_flags;
  vm->address = vaddr;
  vm->size = size;
  vm->virt_size = virt_size;
  return vm;
}

static address_space_t *select_space(address_space_t *user_space, uintptr_t addr) {
  if (addr >= KERNEL_SPACE_START) {
    return kernel_space;
  }
  return user_space;
}

void init_recursive_pgtable() {
  uintptr_t pgtable = get_current_pgtable();
  // identity mappings are still in effect
  uint64_t *table_virt = (void *) pgtable;

  // here we setup recursive paging which enables us to access the containing
  // page for any entry at any level of the hierarchy at an accessible known address.
  table_virt[R_ENTRY] = (uint64_t) pgtable | PE_WRITE | PE_PRESENT;

  // we also setup a fixed page directory pointer table to enable the temporary mapping
  // of pages. each cpu has its own entry in this table which can be accessed with TEMP_PDPTE
  temp_pdpt_page = alloc_pages(1);
  table_virt[T_ENTRY] = temp_pdpt_page->address | PE_WRITE | PE_PRESENT;
  memset(TEMP_PDPT, 0, PAGE_SIZE);
}

static inline uintptr_t choose_best_hint(uintptr_t hint, uint32_t vm_flags) {
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
  //space_lock_assert(space, MA_OWNED);
  if (size > (UINT64_MAX - base) || base + size > space->max_addr) {
    panic("no free address space");
  }

  intvl_node_t *closest_node;
  interval_t intvl = intvl(base, base + size);
  interval_t range = intvl_tree_find_free_gap(space->new_tree, intvl, align, &closest_node);
  if (!closest_node) {
    *closest_vm = NULL;
    return base;
  }

  *closest_vm = closest_node->data;
  return range.start;
}

static bool check_range_free(
  address_space_t *space,
  uintptr_t base,
  size_t size,
  uint32_t vm_flags,
  vm_mapping_t **closest_vm
) {
  // space_lock_assert(space, MA_OWNED);
  intvl_node_t *closest_node;
  interval_t intvl = intvl(base, base + size);
  interval_t result = intvl_tree_find_free_gap(space->new_tree, intvl, 0, &closest_node);
  if (!intvl_eq(intvl, result)) {
    return false;
  }

  if (closest_node != NULL) {
    *closest_vm = closest_node->data;
  }
  return true;
}

static uintptr_t vm_virtual_start(vm_mapping_t *vm) {
  // if the mapping is a stack mapping, vm->address might be above the real start address
  if (vm->flags & VM_STACK) {
    // account for the empty space + the guard page
    size_t empty = vm->virt_size - vm->size;
    return vm->address - empty;
  } else {
    // otherwise the start address is the same as the vm address
    return vm->address;
  }
}

static interval_t vm_virt_interval(vm_mapping_t *vm) {
  uintptr_t start = vm_virtual_start(vm);
  return intvl(start, start + vm->virt_size);
}

void init_address_space() {
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

  init_recursive_pgtable();
}

static bool are_valid_vmap_args(enum vm_type type, uintptr_t hint, size_t size, size_t vm_size, uint32_t vm_flags, void *data) {
  if (vm_size != 0 && vm_size < size) {
    return false;
  }

  size_t pg_size = PAGE_SIZE;
  if (vm_flags & VM_HUGE_2MB) {
    pg_size = PAGE_SIZE_2MB;
    if (vm_flags & VM_HUGE_1GB) {
      EPRINTF("cant have both 2MB and 1GB huge pages\n");
      return false; // cant have both
    }
  } else if (vm_flags & VM_HUGE_1GB) {
    pg_size = PAGE_SIZE_1GB;
  }

  // check compatibility with alt-sized pages
  if (pg_size != PAGE_SIZE && (vm_flags & VM_STACK)) {
    EPRINTF("cant have huge pages with stack mappings\n");
    return false;
  }

  if (vm_flags & VM_FIXED) {
    // make sure hint is aligned and contained within the right address space
    if (vm_flags & VM_USER && !(hint >= USER_SPACE_START && hint < USER_SPACE_END)) {
      EPRINTF("fixed hint for user mapping is not in user space\n");
      return false;
    }
    if (!(vm_flags & VM_USER) && !(hint >= KERNEL_SPACE_START && hint < KERNEL_SPACE_END)) {
      EPRINTF("fixed hint for kernel mapping is not in kernel space\n");
      return false;
    }
  }

  // make sure the sizes are aligned for given page size
  if (!is_aligned(size, pg_size)) {
    EPRINTF("size is not aligned to page size\n");
    return false;
  } else if (!is_aligned(vm_size, pg_size)) {
    EPRINTF("vm_size is not aligned to page size\n");
    return false;
  }

  switch (type) {
    case VM_TYPE_RSVD:
    case VM_TYPE_PHYS:
      return true;
    case VM_TYPE_PAGE: {
      page_t *page = data;
      if (!(page->flags & PG_HEAD)) {
        EPRINTF("page is not a head page\n");
        return false;
      } else if (size != (page->head.count * pg_size)) {
        EPRINTF("pages do not cover the specified mapping size (%zu != %zu)\n", size, page->head.count * pg_size);
        return false;
      }
      return true;
    }
    case VM_TYPE_FILE: {
      vm_file_t *file = data;
      if (size != file->size) {
        EPRINTF("file size does not match the mapping size (%zu != %zu)\n", size, file->size);
        return false;
      }
      return true;
    }
    default:
      unreachable;
  }
  return true;
}

// creates a new virtual mapping. if the vm_user flag is et, the mapping will be
// allocated in the provided address space. if the vm_fixed flag is set, the hint
// address will be used as the base address for the mapping and it will fial if
// the address is not available. By default, the mapping is reflected in the paage
// tables of the current address space, but the vm_nomap flag cn be used to only
// allocate the virtual range. On success, a non-zero virtual address is returned.
static int vmap_internal (
  address_space_t *user_space,
  enum vm_type type,
  uintptr_t hint,
  size_t size,
  size_t vm_size,
  uint32_t vm_flags,
  const char *name,
  void *data,
  uintptr_t *out_vaddr
  ) {
    ASSERT(type < VM_MAX_TYPE);
    if (!are_valid_vmap_args(type, hint, size, vm_size, vm_flags, data))
      return -EINVAL;

    // if any protection is given, the region must be readable
    if (vm_flags & VM_WRITE || vm_flags & VM_EXEC) {
      vm_flags |= VM_READ;
    }

    size_t pg_size = vm_flags_to_size(vm_flags);
    size_t virt_size = max(vm_size, size);
    size_t virt_off;
    uintptr_t virt_base;

    if (vm_flags & VM_FIXED) {
      if (vm_flags & VM_STACK) {
        // a fixed stack mapping uses the hint as the base address of the
        // mapped part even though the actual virt_base address is lower
        //   ^^higher addresses^^
        //     ...
        //   ======= < vm->address+vm->size
        //    space
        //   ------- < vm->address (hint)
        //    guard
        //   -------
        //    empty
        //   ======= < virt_base
        virt_size += PAGE_SIZE; //guard page
        virt_off = (virt_size - size); // empty space + guard
        if (hint < virt_off) {
          // hint is too low
          return -EINVAL;
        }
        virt_base = hint - virt_off;
      } else {
        //   ^^higher addresses^^
        //     ...
        //   ======= < vm->address+vm->vm_size
        //    empty
        //   ------- < vm->address+vm->size
        //    space
        //   ======= < vm->address (hint) = virt_base
        virt_off = 0;
        virt_base = hint;
      }
    } else {
      // dynamic mappings may adhere to the hint if one is given, but it is
      // best-effort
      hint = choose_best_hint(hint, vm_flags);

      if (vm_flags & VM_STACK) {
        virt_size += PAGE_SIZE; // guard page
        virt_off = PAGE_SIZE;
        virt_base = max(hint, virt_size);
      } else {
        virt_off = 0;
        virt_base = hint;
      }
    }

    address_space_t *space = select_space(user_space, virt_base);
    //space_lock(space);
  
    // allocate the virtual space
    int res = 0;
    vm_mapping_t *closest = NULL;
    if (vm_flags & VM_FIXED) {
      // make sure the requested range is free
      if (!check_range_free(space, virt_base, virt_size, vm_flags, &closest)) {
        EPRINTF("requested fixed address range is not free %018p-%018p [name=%s]\n",
                virt_base, virt_base+virt_size, name);
        res = -EADDRNOTAVAIL;
        goto ret;
      }
    } else {
      // dynamically allocated (use virt_base as a starting point)
      virt_base = get_free_region(space, virt_base, virt_size, pg_size, vm_flags, &closest);
      if (virt_base == 0) {
        EPRINTF("failed to satisfy allocation request [name=%s]\n", name);
        res = -ENOMEM;
        goto ret;
      }
    }
  
    // create the vm_mapping struct and add it to the space
    vm_mapping_t *vm = vm_struct_alloc(type, vm_flags, virt_base+virt_off, size, virt_size);
    vm->name = str_from(name);
    vm->space = space;
    switch (type) {
      case VM_TYPE_RSVD: vm->flags &= ~VM_PROT_MASK; break;
      case VM_TYPE_PHYS: vm->vm_phys = (uintptr_t) data; break;
      case VM_TYPE_PAGE: vm->vm_pages = (page_t *) data; break;
      case VM_TYPE_FILE: vm->vm_file = (vm_file_t *) data; break;
      default: unreachable;
    }
  
    intvl_tree_insert(space->new_tree, vm_virt_interval(vm), vm);
    space->num_mappings++;
  
    // add it to the mappings list
    if (closest) {
      LIST_INSERT(&space->mappings, vm, vm_list, closest);
    } else {
      LIST_ADD(&space->mappings, vm, vm_list);
    }
  
    // map the region if any protection flags are given
    if (vm->flags & VM_PROT_MASK) {
      // unless we're asked to skip it
      if (vm->flags & VM_NOMAP) {
        vm->flags ^= VM_NOMAP; // flag only applied on allocation
        vm->flags |= VM_MAPPED;
      } else {
        vm_update_internal(vm, vm->flags);
      }
    }
  
    if (out_vaddr)
      *out_vaddr = virt_base + virt_off;
  
  LABEL(ret);
    space_unlock(space);
    return res;
  }
}

uintptr_t vmap_phys(uintptr_t phys_addr, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name) {
  int res;
  uintptr_t vaddr;
  if ((res = vmap_internal(curspace, VM_TYPE_PHYS, hint, size, size, vm_flags, name, (void *)phys_addr, &vaddr)) {
    ALLOC_ERROR("vmap: failed to make physical address mapping %s phys=%p] {:err}\m", name, phys_addr, res);
    return 0;
  })
  return vaddr;
}
*/