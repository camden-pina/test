#include <mm/pgtable.h>
#include <mm/pmm.h>
#include <string.h>
#include <panic.h>
#include <cpu.h>
#include <fmt.h>
#include <printf.h>

uintptr_t get_current_pgtable();

// Remove redefinitions that conflict with header definitions:
// #define KERNEL_SPACE_START 0xFFFFFF8000000000ULL
// #define PE_FRAME_MASK  0x000FFFFFFFFFF000ULL  // bits 12..51

typedef enum pg_level {
  PG_LEVEL_PT,
  PG_LEVEL_PD,
  PG_LEVEL_PDPT,
  PG_LEVEL_PML4,
  PG_LEVEL_MAX
} pg_level_t;

// We care about these flags: PRESENT, WRITE, USER, WRITE_THROUGH,
// CACHE_DISABLE, NO_EXECUTE, GLOBAL, SIZE.
#define KNOWN_FLAG_MASK  (PE_PRESENT | PE_WRITE | PE_USER | PE_WRITE_THROUGH | PE_CACHE_DISABLE | PE_GLOBAL | PE_SIZE | PE_NO_EXECUTE)


// Reentrant version: Converts a 16-bit flag value (including NX) into a human-readable string.
// The output is written into the provided buffer.
typedef struct {
    uint16_t mask;
    const char *name;
} flag_map_t;

void flags_to_str_r(uint16_t flags, char *buf, size_t bufsize) {
    static const flag_map_t flag_table[] = {
        {PE_PRESENT,       "P"},
        {PE_WRITE,         "W"},
        {PE_USER,          "U"},
        {PE_CACHE_DISABLE, "NC"},
        {PE_WRITE_THROUGH, "WT"},
        {PE_GLOBAL,        "G"},
        {PE_SIZE,          "S"},
    };

    size_t len = 0;

    if (bufsize == 0)
        return;

    buf[0] = '\0';

    for (size_t i = 0; i < sizeof(flag_table) / sizeof(flag_table[0]); ++i) {
        if (flags & flag_table[i].mask) {
            int written = ksnprintf(buf + len, bufsize - len, "%s ", flag_table[i].name);
            if (written < 0 || (size_t)written >= bufsize - len)
                return; // Truncated
            len += written;
        }
    }

    // Add hex
    int written = ksnprintf(buf + len, bufsize - len, "(0x%04X)", flags);
    if (written < 0 || (size_t)written >= bufsize - len)
        return;
    len += written;

    // Add NX if present
    if (flags & PE_NO_EXECUTE) {
        written = ksnprintf(buf + len, bufsize - len, " NX");
        if (written < 0 || (size_t)written >= bufsize - len)
            return;
        len += written;
    }
}

// A helper wrapper for legacy calls (if needed).
const char *flags_to_str(uint16_t flags) {
    // Not reentrant! Use only when you call it once.
    static char buf[128];
    flags_to_str_r(flags, buf, sizeof(buf));
    return buf;
}

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

inline uint16_t vm_flags_to_pe_flags(uint32_t vm_flags) {
    uint16_t entry_flags = PE_PRESENT;  // Always mark present

    if (vm_flags & VM_WRITE)
        entry_flags |= PE_WRITE;

    if (vm_flags & VM_USER)
        entry_flags |= PE_USER;

    if (vm_flags & VM_NOCACHE)
        entry_flags |= PE_CACHE_DISABLE;

    if (vm_flags & VM_WRITETHRU)
        entry_flags |= PE_WRITE_THROUGH;

    if (!(vm_flags & VM_EXEC))
        entry_flags |= PE_NO_EXECUTE;

    if (vm_flags & VM_GLOBAL)
        entry_flags |= PE_GLOBAL;

    if (vm_flags & (VM_HUGE_2MB | VM_HUGE_1GB))
        entry_flags |= PE_SIZE;

    return entry_flags;
}

#ifndef KERNEL_SPACE_START
#error "KERNEL_SPACE_START must be defined in included headers"
#endif

/*===========================================================================
 * Low-Level Mapping Function: early_map_entry
 *
 * Maps a single page (or huge page) at virt_addr with physical address phys_addr.
 * If an entry already exists:
 *   - If the physical frame matches and the flag bits are identical, return it.
 *   - If the physical frame matches but the flags differ:
 *       * If VM_OVERWRITE is set, update the flags to the new value.
 *       * Otherwise, print a warning.
 *   - Otherwise, panic.
 *===========================================================================*/
uint64_t *early_map_entry(uintptr_t virt_addr, intptr_t phys_addr, uint32_t vm_flags) {
    kassert(virt_addr % PAGE_SIZE == 0);
    kassert(phys_addr % PAGE_SIZE == 0);

    pg_level_t map_level = PG_LEVEL_PT;
    if (vm_flags & VM_HUGE_2MB) {
        kassert(is_aligned(virt_addr, SIZE_2MB));
        kassert(is_aligned(phys_addr, SIZE_2MB));
        map_level = PG_LEVEL_PD;
    }

    uint16_t desired_flags = vm_flags_to_pe_flags(vm_flags);
    uint64_t *pml4 = (uint64_t *)boot_info_v2->pml4_addr;
    uint64_t *table = pml4;

    for (pg_level_t level = PG_LEVEL_PML4; level > map_level; level--) {
        int index = index_for_pg_level(virt_addr, level);
        uintptr_t next_table = table[index] & PE_FRAME_MASK;

        if (next_table == 0) {
            uintptr_t new_table = pmm_early_alloc_pages(1);
            memset((void *)new_table, 0, PAGE_SIZE);
            table[index] = new_table | PE_WRITE | PE_PRESENT;
            next_table = new_table;
        } else if (!(table[index] & PE_PRESENT)) {
            table[index] = next_table | PE_WRITE | PE_PRESENT;
        }
        table = (uint64_t *)next_table;
    }

    int index = index_for_pg_level(virt_addr, map_level);
    if (table[index] & PE_PRESENT) {
        if ((table[index] & PE_FRAME_MASK) == (uintptr_t)phys_addr) {
            uint16_t existing_flags = table[index] & 0xFFFF;
            if (existing_flags != desired_flags) {
                if (vm_flags & VM_OVERWRITE) {
                    /* Overwrite the flags with the new desired flags */
                    table[index] = (table[index] & PE_FRAME_MASK) | desired_flags;
                    kprintf("INFO: early_map_entry: overwritten flags at 0x%llx to new value.\n", virt_addr);
                } else {
                    char existing_buf[128], desired_buf[128];
                    flags_to_str_r(existing_flags, existing_buf, sizeof(existing_buf));
                    flags_to_str_r(desired_flags, desired_buf, sizeof(desired_buf));
                    kprintf("WARNING: early_map_entry: flag mismatch at 0x%llx, existing flags: [%s], desired flags: [%s]\n",
                            virt_addr, existing_buf, desired_buf);
                }
            }
            return table + index;
        }
        panic("early_map_entry: conflicting mapping exists at 0x%llx (existing entry=0x%llx, requested phys=0x%llx)",
              virt_addr, table[index], phys_addr);
    }
    table[index] = phys_addr | desired_flags;
    return table + index;
}

/*===========================================================================
 * High-Level Mapping Function: early_map_entries
 *
 * Maps 'count' pages (or huge pages) starting at physical address paddr into
 * virtual address starting at vaddr.
 *===========================================================================*/
void *early_map_entries(uintptr_t vaddr, uintptr_t paddr, size_t count, uint32_t vm_flags) {
    extern address_space_t *cur_space;
    if (!cur_space) {
        static address_space_t early_space;
        if (early_space.max_addr == 0) {
            early_space.min_addr = KERNEL_SPACE_START;
            early_space.max_addr = KERNEL_SPACE_END;
            early_space.page_table = get_current_pgtable();
            early_space.num_mappings = 0;
            LIST_INIT(&early_space.mappings);
        }
        cur_space = &early_space;
    }

    kassert(vaddr % PAGE_SIZE == 0);
    kassert(paddr % PAGE_SIZE == 0);
    kassert(count > 0);

    pg_level_t map_level = PG_LEVEL_PT;
    size_t stride = PAGE_SIZE;
    if (vm_flags & VM_HUGE_2MB) {
        kassert(vaddr % SIZE_2MB == 0);
        kassert(paddr % SIZE_2MB == 0);
        map_level = PG_LEVEL_PD;
        stride = SIZE_2MB;
    }

    kassert(vaddr + (count * stride) > vaddr && "early_map_entries: arithmetic overflow detected");
    if (vaddr + (count * stride) > cur_space->max_addr) {
        panic("early_map_entries: mapping from 0x%llx for %zu pages exceeds current space max 0x%llx",
              vaddr, count, cur_space->max_addr);
    }

    void *addr = (void *)vaddr;
    uint16_t desired_flags = vm_flags_to_pe_flags(vm_flags);

    while (count > 0) {
        int index = index_for_pg_level(vaddr, map_level);
        uint64_t *entry = early_map_entry(vaddr, paddr, vm_flags);
        entry++;  // advance to the next slot in the table

        count--;
        vaddr += stride;
        paddr += stride;

        for (int i = index + 1; i < 512 && count > 0; i++) {
            if (vaddr + stride > cur_space->max_addr) {
                panic("early_map_entries: inner loop mapping exceeds bounds at vaddr 0x%llx", vaddr);
            }
            if (*entry & PE_PRESENT) {
                if (((*entry) & PE_FRAME_MASK) != paddr) {
                    panic("early_map_entries: conflicting mapping exists at 0x%llx (entry=0x%llx, requested phys=0x%llx)",
                          vaddr, *entry, paddr);
                }
                uint16_t existing_flags = *entry & KNOWN_FLAG_MASK;
                if (existing_flags != (desired_flags & KNOWN_FLAG_MASK)) {
                    if (vm_flags & VM_OVERWRITE) {
                        *entry = paddr | desired_flags;
                        kprintf("INFO: early_map_entries: overwritten flags at 0x%llx to new value.\n", vaddr);
                    } else {
                        char existing_buf[128], desired_buf[128];
                        flags_to_str_r(existing_flags, existing_buf, sizeof(existing_buf));
                        flags_to_str_r(desired_flags, desired_buf, sizeof(desired_buf));
                        kprintf("WARNING: early_map_entries: flag mismatch at 0x%llx, existing=[%s], desired=[%s]\n",
                                vaddr, existing_buf, desired_buf);
                    }
                }
            } else {
                *entry = paddr | desired_flags;
            }
            entry++;
            count--;
            vaddr += stride;
            paddr += stride;
            cpu_invlpg(vaddr);
        }
    }
    return addr;
}

// Robust virtual-to-physical conversion.
uintptr_t virt_to_phys(void *virt_address) {
    if (!boot_info_v2 || !boot_info_v2->pml4_addr) {
        kprintf("virt_to_phys: boot_info_v2 or its pml4_addr is NULL!\n");
        return 0;
    }

    uintptr_t virt_addr = (uintptr_t)virt_address;
    uint64_t *pml4 = (uint64_t *)boot_info_v2->pml4_addr;
    uint64_t *table = pml4;

    for (int level = PG_LEVEL_PML4; level > PG_LEVEL_PT; level--) {
        int index = index_for_pg_level(virt_addr, level);
        uint64_t entry = table[index];
        if (!(entry & PE_PRESENT)) {
            kprintf("virt_to_phys: Level %d, index %d not present (entry=0x%llx)\n",
                    level, index, entry);
            return 0;
        }
        if (entry & PE_SIZE) {
            uint64_t page_frame = entry & PE_FRAME_MASK;
            uint64_t offset = virt_addr & ((1ULL << pg_level_to_shift(level)) - 1);
            return page_frame | offset;
        }
        table = (uint64_t *)(entry & PE_FRAME_MASK);
    }

    int index = index_for_pg_level(virt_addr, PG_LEVEL_PT);
    uint64_t entry = table[index];
    if (!(entry & PE_PRESENT)) {
        kprintf("virt_to_phys: PT level, index %d not present (entry=0x%llx)\n",
                index, entry);
        return 0;
    }

    return (entry & PE_FRAME_MASK) | (virt_addr & 0xFFF);
}

uintptr_t get_current_pgtable() {
    return __read_cr3() & PE_FRAME_MASK;
}