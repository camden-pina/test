#include <mm/pgtable.h>
#include <mm/pmm.h>
#include <string.h>
#include <panic.h>
#include <cpu.h>

uintptr_t get_current_pgtable();

#define NUM_ENTRIES 512
#define T_ENTRY 509ULL // temp pdpt entry index
#define R_ENTRY 510ULL // recursive entry index

#define PT_INDEX(a) (((a) >> 12) & 0x1FF)
#define PDT_INDEX(a) (((a) >> 21) & 0x1FF)
#define PD_INDEX(addr) (((addr) >> 21) & 0x1FF)

#define PDPT_INDEX(a) (((a) >> 30) & 0x1FF)
#define PML4_INDEX(a) (((a) >> 39) & 0x1FF)

typedef enum pg_level {
  PG_LEVEL_PT,
  PG_LEVEL_PD,
  PG_LEVEL_PDPT,
  PG_LEVEL_PML4,
  PG_LEVEL_MAX
} pg_level_t;

int index_for_pg_level(uintptr_t virt_addr, int level) {
    switch (level) {
        case PG_LEVEL_PML4:
            return (virt_addr >> 39) & 0x1FF;
        case PG_LEVEL_PDPT:
            return (virt_addr >> 30) & 0x1FF;
        case PG_LEVEL_PD:
            return (virt_addr >> 21) & 0x1FF;
        case PG_LEVEL_PT:
            return (virt_addr >> 12) & 0x1FF;
        default:
            // Unsupported level.
            return 0;
    }
}

int pg_level_to_shift(int level) {
    switch (level) {
        case PG_LEVEL_PML4:
            return 39;
        case PG_LEVEL_PDPT:
            return 30;
        case PG_LEVEL_PD:
            return 21;
        case PG_LEVEL_PT:
            return 12;
        default:
            return 0;
    }
}



// pdpe page for the temp entry
page_t *temp_pdpt_page;

static inline uint16_t vm_flags_to_pe_flags(uint32_t vm_flags) {
    uint16_t entry_flags = PE_PRESENT;
    entry_flags |= (vm_flags & VM_WRITE) ? PE_WRITE : 0;
    entry_flags |= (vm_flags & VM_USER) ? PE_USER : 0;
    entry_flags |= (vm_flags & VM_NOCACHE) ? PE_CACHE_DISABLE : 0;
    entry_flags |= (vm_flags & VM_WRITETHRU) ? PE_WRITE_THROUGH : 0;
    entry_flags |= (vm_flags & VM_EXEC) ? 0 : PE_NO_EXECUTE;
    entry_flags |= (vm_flags & VM_GLOBAL) ? PE_GLOBAL : 0;
    if ((vm_flags & VM_HUGE_2MB) || (vm_flags & VM_HUGE_1GB)) {
      entry_flags |= PE_SIZE;
    }
    return entry_flags;
  }

uint64_t *early_map_entry(uintptr_t virt_addr, intptr_t phys_addr, uint32_t vm_flags) {
    kassert(virt_addr % PAGE_SIZE == 0);
    kassert(phys_addr % PAGE_SIZE == 0);

    pg_level_t map_level = PG_LEVEL_PT;
    if (vm_flags & VM_HUGE_2MB) {
        kassert(is_aligned(virt_addr, SIZE_2MB));
        kassert(is_aligned(phys_addr, SIZE_2MB));
        map_level = PG_LEVEL_PD;
    }

    uint16_t entry_flags = vm_flags_to_pe_flags(vm_flags);
    uint64_t *pml4 = (void *) ((uint64_t) boot_info_v2->pml4_addr);
    uint64_t *table = pml4;

    for (pg_level_t level = PG_LEVEL_PML4; level > map_level; level--) {
        int index = index_for_pg_level(virt_addr, level);
        uintptr_t next_table = table[index] & PE_FRAME_MASK;

        if (next_table == 0) {
            // create new table
            uintptr_t new_table = pmm_early_alloc_pages(1);
            memset((void *)new_table, 0, PAGE_SIZE);
            table[index] = new_table | PE_WRITE | PE_PRESENT;
            next_table = new_table;
        } else if (!(table[index] & PE_PRESENT)) {
            table[index] = next_table | PE_WRITE | PE_PRESENT;
        }
        table = (void *)next_table;
    }
    int index = index_for_pg_level(virt_addr, map_level);
    table[index] = phys_addr | entry_flags;
    return table + index;
}

void *early_map_entries(uintptr_t vaddr, uintptr_t paddr, size_t count, uint32_t vm_flags) {
    kassert(vaddr % PAGE_SIZE == 0);
    kassert(paddr % PAGE_SIZE == 0);
    kassert(count > 0);

    pg_level_t map_level = PG_LEVEL_PT;
    size_t stride = PAGE_SIZE;

    if (vm_flags & VM_HUGE_2MB) {
        kassert(is_aligned(vaddr, SIZE_2MB));
        kassert(is_aligned(paddr, SIZE_2MB));
        map_level = PG_LEVEL_PD;
        stride = SIZE_2MB;
    }

    void *addr = (void *) vaddr;
    uint16_t entry_flags = vm_flags_to_pe_flags(vm_flags);

    while (count > 0) {
        int index = index_for_pg_level(vaddr, map_level);
        uint64_t *entry = early_map_entry(vaddr, paddr, vm_flags);
        entry++;
        count--;
        vaddr += stride;
        paddr += stride;


        for (int i = index + 1; i < NUM_ENTRIES; i++) {
            if (count == 0) {
                break;
            }

            *entry = paddr | entry_flags;
            entry++;
            count--;
            vaddr += stride;
            paddr += stride;
            cpu_invlpg(vaddr);
        }
    }
    return addr;
}

uintptr_t next_free_virt = 0xFFFFFF8000400000; // 0xFFFFC01000000000ULL; // 0xFFFFFF8000D00000; // Start dynamic virtual mappings here

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

void *alloc_virt_mem(size_t size, uint32_t vm_flags) {
    kassert(size > 0);
    size = ALIGN_UP(size, PAGE_SIZE); // ensure it's page-aligned

    uintptr_t vaddr = next_free_virt;
    uintptr_t paddr = pmm_early_alloc_pages(size / PAGE_SIZE);
    if (!paddr) {
        panic("Out of physical memory");
    }
    // map allocated physical pages to virtual memory
    early_map_entries(vaddr, paddr, size / PAGE_SIZE, vm_flags);

    next_free_virt += size; // move to next free virtual memory region
    return (void *)vaddr;
}

/*
uintptr_t virt_to_phys(void *virt_address) {
    uintptr_t virt_addr = (uintptr_t)virt_address;
    uint64_t *pml4 = (void *) ((uint64_t)boot_info_v2->pml4_addr);
    uint64_t *table = pml4;

    for (pg_level_t level = PG_LEVEL_PML4; level > PG_LEVEL_PT; level--) {
        int index = index_for_pg_level(virt_addr, level);
        if (!(table[index] & PE_PRESENT)) return 0; // Not mapped
        if (table[index] & PE_SIZE) {  // Handle huge pages
            return (table[index] & PE_FRAME_MASK) | (virt_addr & ((1ULL << pg_level_to_shift(level)) - 1));
        }
        table = (uint64_t *)(table[index] & PE_FRAME_MASK);
    }

    int index = index_for_pg_level(virt_addr, PG_LEVEL_PT);
    if (!(table[index] & PE_PRESENT)) return 0; // Not mapped

    return (table[index] & PE_FRAME_MASK) | (virt_addr & 0xFFF);
}
    */

#define PE_FRAME_MASK  0x000FFFFFFFFFF000ULL  // bits 12..51

// The robust virtual-to-physical conversion function.
uintptr_t virt_to_phys(void *virt_address) {
    if (!boot_info_v2 || !boot_info_v2->pml4_addr) {
        kprintf("virt_to_phys: boot_info_v2 or its pml4_addr is NULL!\n");
        return 0;
    }

    uintptr_t virt_addr = (uintptr_t)virt_address;
    uint64_t *pml4 = (uint64_t *)boot_info_v2->pml4_addr;
    uint64_t *table = pml4;

    // Iterate from PML4 level down to the page table (PT) level.
    for (int level = PG_LEVEL_PML4; level > PG_LEVEL_PT; level--) {
        int index = index_for_pg_level(virt_addr, level);
        uint64_t entry = table[index];
        if (!(entry & PE_PRESENT)) {
            kprintf("virt_to_phys: Level %d, index %d not present (entry=0x%llx)\n",
                    level, index, entry);
            return 0;
        }
        // If the entry indicates a huge page, compute the physical address.
        if (entry & PE_SIZE) {
            uint64_t page_frame = entry & PE_FRAME_MASK;
            uint64_t offset = virt_addr & ((1ULL << pg_level_to_shift(level)) - 1);
            return page_frame | offset;
        }
        // Otherwise, move to the next lower level.
        table = (uint64_t *)(entry & PE_FRAME_MASK);
    }

    // Now at the page table level.
    int index = index_for_pg_level(virt_addr, PG_LEVEL_PT);
    uint64_t entry = table[index];
    if (!(entry & PE_PRESENT)) {
        kprintf("virt_to_phys: PT level, index %d not present (entry=0x%llx)\n",
                index, entry);
        return 0;
    }

    return (entry & PE_FRAME_MASK) | (virt_addr & 0xFFF);
}