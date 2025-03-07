#ifndef _KERNEL_PGTABLE_H
#define _KERNEL_PGTABLE_H

#include <stdint.h>
#include <stddef.h>

#define get_virt_addr(l4, l3, l2, l1) \
  ((0xFFFFULL << 48) | ((uint64_t)(l4) << 39) | ((uint64_t)(l3) << 30) | \
  ((uint64_t)(l2) << 21) | ((uint64_t)(l1) << 12))

#define PML4_PTR    ((uint64_t *) get_virt_addr(R_ENTRY, R_ENTRY, R_ENTRY, R_ENTRY))
#define TEMP_PDPT   ((uint64_t *) get_virt_addr(R_ENTRY, R_ENTRY, R_ENTRY, T_ENTRY))
#define TEMP_PDPTE  (&TEMP_PDPT[curcpu_id])
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

typedef volatile int refcount_t;

#define _refname refcount
#define _refcount refcount_t _refname

typedef struct page {
    uint64_t address;             // physical frame
    uint32_t flags;               // page flags
    struct {                      // *** valid if PG_HEAD ***
      uint64_t count : 63;        //   number of pages in the list
      uint64_t contiguous : 1;    //   whether the list is physically contiguous
    } head;
    union {
      struct page *source;        // source page ref (if PG_COW)
    };
    struct pte *entries;          // s-list of pte structs (l)
    struct page *next;            // next page ref (l)
    _refcount;
  } page_t;

uint64_t *early_map_entry(uintptr_t virt_addr, intptr_t phys_addr, uint32_t vm_flags);
void *early_map_entries(uintptr_t vaddr, uintptr_t paddr, size_t count, uint32_t vm_flags);
void *alloc_virt_mem(size_t size, uint32_t vm_flags);
uintptr_t pmm_virt_to_phys(void *vaddr);
uintptr_t virt_to_phys(void *virt_address);

#endif
