#ifndef _KERNEL_PGTABLE_H
#define _KERNEL_PGTABLE_H

#include <stdint.h>
#include <stddef.h>
#include <mm_types.h>
#include <percpu.h>

#define get_virt_addr(l4, l3, l2, l1) \
  ((0xFFFFULL << 48) | ((uint64_t)(l4) << 39) | ((uint64_t)(l3) << 30) | \
  ((uint64_t)(l2) << 21) | ((uint64_t)(l1) << 12))

  #define T_ENTRY 509ULL // temp pdpt entry index
#define R_ENTRY 510ULL // recursive entry index

#define PML4_PTR    ((uint64_t *) get_virt_addr(R_ENTRY, R_ENTRY, R_ENTRY, R_ENTRY))
#define TEMP_PDPT   ((uint64_t *) get_virt_addr(R_ENTRY, R_ENTRY, R_ENTRY, T_ENTRY))
#define TEMP_PDPTE  (&TEMP_PDPT[curcpu_id])
#define TEMP_PDPTE  (&PML4_PTR[T_ENTRY])

#define TEMP_PTR    ((uint64_t *) get_virt_addr(R_ENTRY, R_ENTRY, T_ENTRY, curcpu_id))

// page entry flags
#define PE_PRESENT        (1ULL << 0)
#define PE_WRITE          (1ULL << 1)
#define PE_USER           (1ULL << 2)
#define PE_WRITE_THROUGH  (1ULL << 3)
#define PE_CACHE_DISABLE  (1ULL << 4)
#define PE_ACCESSED       (1ULL << 5)
#define PE_DIRTY          (1ULL << 6)
#define PE_SIZE           (1ULL << 7)
#define PE_GLOBAL         (1ULL << 8)
#define PE_NO_EXECUTE     (1ULL << 63)

#define PE_FLAGS_MASK 0xFFF
#define PE_FRAME_MASK 0xFFFFFFFFFFFFF000

typedef enum pg_level {
  PG_LEVEL_PT,
  PG_LEVEL_PD,
  PG_LEVEL_PDPT,
  PG_LEVEL_PML4,
  PG_LEVEL_MAX
} pg_level_t;

void *early_map_entries(uintptr_t vaddr, uintptr_t paddr, size_t count, uint32_t vm_flags);
uintptr_t pmm_virt_to_phys(void *vaddr);
uintptr_t virt_to_phys(void *virt_address);

uintptr_t get_current_pgtable();
void set_current_pgtable(uintptr_t table_phys);
void flags_to_str_r(uint16_t flags, char *buf, size_t bufsize);
uint16_t vm_flags_to_pe_flags(uint32_t vm_flag);

  uint64_t recursive_duplicate_pgtable(
    pg_level_t level,
    uint64_t *dest_parent_table,
    uint64_t *src_parent_table,
    uint16_t index,
    page_t **out_pages
  );

/*
  * fork_page_tables - Duplicate the current page tables (for fork).
  *
  * @out_pages:     Pointer to a list head that will contain all meta pages allocated
  *                 during the page table fork.
  * @deepcopy_user: If true, user-space entries will be deeply copied (otherwise a shallow copy is done).
  *
  * This function creates a new PML4 (by allocating one page) and then:
  *   - Temporarily maps it via a reserved PDPTE entry.
  *   - Clears its contents.
  *   - Shallow-copies kernel-space entries.
  *   - Optionally deep-copies user-space entries via recursive duplication.
  *   - Unmaps the temporary mapping.
  *
  * The new PML4’s physical address is returned.
  */
 uintptr_t fork_page_tables(page_t **out_pages, bool deepcopy_user);

void init_recursive_pgtable();

#endif
