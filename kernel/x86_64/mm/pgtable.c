
#include <mm/pgtable.h>
#include <mm/pmm.h>
#include <string.h>
#include <panic.h>
#include <cpu.h>
#include <fmt.h>
#include <printf.h>
#include <cpu.h>

uintptr_t get_current_pgtable();

// Remove redefinitions that conflict with header definitions:
// #define KERNEL_SPACE_START 0xFFFFFF8000000000ULL
// #define PE_FRAME_MASK  0x000FFFFFFFFFF000ULL  // bits 12..51

#define PT_INDEX(a) (((a) >> 12) & 0x1FF)
#define PDT_INDEX(a) (((a) >> 21) & 0x1FF)
#define PDPT_INDEX(a) (((a) >> 30) & 0x1FF)
#define PML4_INDEX(a) (((a) >> 39) & 0x1FF)

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

#define NUM_ENTRIES 512

#define ASSERT(x) kassert(x)

static uint64_t *early_map_entry(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t vm_flags) {
    ASSERT(virt_addr % PAGE_SIZE == 0);
    ASSERT(phys_addr % PAGE_SIZE == 0);
  
    pg_level_t map_level = PG_LEVEL_PT;
    if (vm_flags & VM_HUGE_2MB) {
      ASSERT(is_aligned(virt_addr, SIZE_2MB));
      ASSERT(is_aligned(phys_addr, SIZE_2MB));
      map_level = PG_LEVEL_PD;
    } else if (vm_flags & VM_HUGE_1GB) {
      ASSERT(is_aligned(virt_addr, SIZE_1GB));
      ASSERT(is_aligned(phys_addr, SIZE_1GB));
      map_level = PG_LEVEL_PDPT;
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
        memset((void *) new_table, 0, PAGE_SIZE);
        table[index] = new_table | PE_WRITE | PE_PRESENT;
        next_table = new_table;
      } else if (!(table[index] & PE_PRESENT)) {
        table[index] = next_table | PE_WRITE | PE_PRESENT;
      }
  
      table = (void *) next_table;
    }
  
    int index = index_for_pg_level(virt_addr, map_level);
    table[index] = phys_addr | entry_flags;
    return table + index;
  }
  
  //
  
  void *early_map_entries(uintptr_t vaddr, uintptr_t paddr, size_t count, uint32_t vm_flags) {
    ASSERT(vaddr % PAGE_SIZE == 0);
    ASSERT(paddr % PAGE_SIZE == 0);
    ASSERT(count > 0);
  
    pg_level_t map_level = PG_LEVEL_PT;
    size_t stride = PAGE_SIZE;
    if (vm_flags & VM_HUGE_2MB) {
      ASSERT(is_aligned(vaddr, SIZE_2MB));
      ASSERT(is_aligned(paddr, SIZE_2MB));
      map_level = PG_LEVEL_PD;
      stride = SIZE_2MB;
    } else if (vm_flags & VM_HUGE_1GB) {
      ASSERT(is_aligned(vaddr, SIZE_1GB));
      ASSERT(is_aligned(paddr, SIZE_1GB));
      map_level = PG_LEVEL_PDPT;
      stride = SIZE_1GB;
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

void set_current_pgtable(uintptr_t table_phys) {
    __write_cr3((uint64_t) table_phys);
    cpu_flush_tlb();
  }

  static inline uint64_t *get_child_pgtable_address(const uint64_t *parent, pg_level_t level, uint16_t index) {
    uintptr_t addr = (uintptr_t) parent;
    addr |= (index << pg_level_to_shift(level));
    return (uint64_t *) addr;
  }

  uint64_t recursive_duplicate_pgtable(
    pg_level_t level,
    uint64_t *dest_parent_table,
    uint64_t *src_parent_table,
    uint16_t index,
    page_t **out_pages
  ) {
    if (level == PG_LEVEL_PT) {
      return src_parent_table[index];
    } else if ((src_parent_table[index] & PE_PRESENT) == 0) {
      return 0;
    }
  
    LIST_HEAD(page_t) table_pages = {0};
    page_t *dest_page = alloc_pages(1);
    dest_parent_table[index] = dest_page->address | PE_WRITE | PE_PRESENT;
    SLIST_ADD(&table_pages, dest_page, next);
  
    uint64_t *src_table = get_child_pgtable_address(src_parent_table, index, level);
    uint64_t *dest_table = get_child_pgtable_address(dest_parent_table, index, level);
    for (int i = 0; i < NUM_ENTRIES; i++) {
      page_t *page_ptr = NULL;
      dest_table[i] = recursive_duplicate_pgtable(level - 1, dest_table, src_table, i, &page_ptr);
      if (page_ptr != NULL) {
        SLIST_ADD_SLIST(&table_pages, page_ptr, SLIST_GET_LAST(page_ptr, next), next);
      }
    }
  
    if (out_pages != NULL) {
      *out_pages = LIST_FIRST(&table_pages);
    }
  
    uint16_t flags = src_parent_table[index] & PE_FLAGS_MASK;
    return dest_page->address | flags;
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

  #include <mm/pgtable.h>
#include <mm/pmm.h>
#include <mm_types.h>
#include <panic.h>
#include <cpu.h>
#include <string.h>
#include <kernel.h>    // For boot_info_v2 or similar if needed
#include <printf.h>

// If not already defined, here is a typical macro:
#ifndef PML4_INDEX
#define PML4_INDEX(addr)  ( ((addr) >> 39) & 0x1FF )
#endif
#ifndef PDPT_INDEX
#define PDPT_INDEX(addr)  ( ((addr) >> 30) & 0x1FF )
#endif
#ifndef PD_INDEX
#define PD_INDEX(addr)    ( ((addr) >> 21) & 0x1FF )
#endif
#ifndef PT_INDEX
#define PT_INDEX(addr)    ( ((addr) >> 12) & 0x1FF )
#endif

// Pick a special kernel VA that you know is *not* used by anything else:
#define KMAP_SINGLE_VA   0xFFFFFF7FA0000000ULL

static bool g_kmap_in_use = false;  // track if our single kmap slot is busy

/* 
 * walk_kernel_page_table - 4-level walk in the current CR3 to find the PTE
 *    for 'virt_addr'. If 'create' is true, we allocate new intermediate pages
 *    as needed. If 'create' is false, we return NULL if the path is missing.
 *
 * NOTE: In many kernels, you keep a direct or “recursive” mapping for all
 * page‐table pages. Below, we do a naive version that tries to cast the
 * physical addresses to virtual addresses. If your kernel does *not* provide
 * a direct mapping, you might need to kmap each intermediate level as well.
 * That is more elaborate, but the principle is the same.
 */
static uint64_t *walk_kernel_page_table(uintptr_t virt_addr, bool create);

/*
 * map_kernel_page - create or update a 4KB mapping:
 *    kernel_va -> phys_addr, with specified PDE flags (PE_WRITE, etc.).
 */
static void map_kernel_page(uintptr_t kernel_va, uintptr_t phys_addr, uint64_t flags);

/*
 * unmap_kernel_page - remove the 4KB mapping for kernel_va in current CR3.
 */
static void unmap_kernel_page(uintptr_t kernel_va);

/*
 * kmap_single_page - map 'phys_addr' into KMAP_SINGLE_VA for temporary access.
 *   'flags' are typical PDE bits like PE_WRITE, PE_CACHE_DISABLE, etc.
 */
void *kmap_single_page(uintptr_t phys_addr, uint16_t flags)
{
    if (g_kmap_in_use) {
        panic("kmap_single_page: single slot already in use!");
    }
    g_kmap_in_use = true;

    // Always set PRESENT. Combine with caller’s flags:
    uint64_t entry_flags = PE_PRESENT | flags;

    map_kernel_page(KMAP_SINGLE_VA, phys_addr, entry_flags);
    cpu_invlpg(KMAP_SINGLE_VA);

    return (void *)KMAP_SINGLE_VA;
}

/*
 * kunmap_single_page - unmap the single kmap slot
 */
void kunmap_single_page(void *va)
{
    if ((uintptr_t)va != (uintptr_t)KMAP_SINGLE_VA) {
        panic("kunmap_single_page: invalid VA");
    }
    unmap_kernel_page(KMAP_SINGLE_VA);
    cpu_invlpg(KMAP_SINGLE_VA);
    g_kmap_in_use = false;
}

/*
 * A small helper that allocates one physical page for a page table,
 * zeroes it out, and returns its physical address.
 */
static inline uintptr_t allocate_table_page(void)
{
    page_t *pg = alloc_pages(1);
    if (!pg) {
        panic("allocate_table_page: out of memory");
    }
    // Zero it out by mapping it temporarily:
    void *va = kmap_single_page(pg->address, PE_WRITE);
    memset(va, 0, PAGE_SIZE);
    kunmap_single_page(va);

    return pg->address;
}

/*
 * walk_kernel_page_table implementation (simplified).
 *
 * The “4-level” approach is:
 *    1) read CR3 => physical address for top-level (PML4).
 *    2) transform that into a *virtual* pointer to the PML4 (if your kernel
 *       has direct or some stable way to see it).
 *    3) look at pml4[index], if empty => allocate next level if 'create'.
 *    4) proceed similarly for PDPT, PD, PT.
 *    5) return &pt[index].
 *
 * IMPORTANT: If your kernel does NOT have a direct mapping for the newly
 * allocated PDE pages, you need to “kmap_single_page” them as well. Below
 * we do a naive cast, which requires that those addresses are in a region
 * your kernel can see by normal pointer arithmetic.
 */
static uint64_t *walk_kernel_page_table(uintptr_t virt_addr, bool create)
{
    // (1) The current CR3 points to the top-level PML4 physical page.
    uintptr_t pml4_phys = get_current_pgtable();
    if (!pml4_phys) {
        panic("walk_kernel_page_table: current_pgtable=0");
    }

    // If your kernel has “boot_info_v2->pml4_addr” as a stable *virtual* address
    // for that same physical page, do something like:
    uint64_t *pml4_virt = (uint64_t *)boot_info_v2->pml4_addr;

    int i_pml4 = PML4_INDEX(virt_addr);
    int i_pdpt = PDPT_INDEX(virt_addr);
    int i_pd   = PD_INDEX(virt_addr);
    int i_pt   = PT_INDEX(virt_addr);

    // Step 1: PML4
    uint64_t entry = pml4_virt[i_pml4];
    if (!(entry & PE_PRESENT)) {
        if (!create) return NULL;
        uintptr_t new_pdpt_phys = allocate_table_page();
        // Mark it present, writable, user/supervisor as needed. Typically kernel => no user bit:
        pml4_virt[i_pml4] = new_pdpt_phys | PE_PRESENT | PE_WRITE;
        entry = pml4_virt[i_pml4];
    }
    uintptr_t pdpt_phys = entry & PE_FRAME_MASK;
    // next level pointer:
    uint64_t *pdpt_virt = (uint64_t*) pdpt_phys;  // naive cast, see note above

    // Step 2: PDPT
    entry = pdpt_virt[i_pdpt];
    if (!(entry & PE_PRESENT)) {
        if (!create) return NULL;
        uintptr_t new_pd_phys = allocate_table_page();
        pdpt_virt[i_pdpt] = new_pd_phys | PE_PRESENT | PE_WRITE;
        entry = pdpt_virt[i_pdpt];
    }
    uintptr_t pd_phys = entry & PE_FRAME_MASK;
    uint64_t *pd_virt = (uint64_t*) pd_phys;

    // Step 3: PD
    entry = pd_virt[i_pd];
    if (!(entry & PE_PRESENT)) {
        if (!create) return NULL;
        uintptr_t new_pt_phys = allocate_table_page();
        pd_virt[i_pd] = new_pt_phys | PE_PRESENT | PE_WRITE;
        entry = pd_virt[i_pd];
    }
    uintptr_t pt_phys = entry & PE_FRAME_MASK;
    uint64_t *pt_virt = (uint64_t*) pt_phys;

    // Step 4: PT
    return &pt_virt[i_pt];
}

static void map_kernel_page(uintptr_t kernel_va, uintptr_t phys_addr, uint64_t flags)
{
    // find or create the PTE
    uint64_t *pte = walk_kernel_page_table(kernel_va, true /* create */);
    if (!pte) {
        panic("map_kernel_page: walk returned NULL");
    }
    *pte = (phys_addr & PE_FRAME_MASK) | (flags & PE_FLAGS_MASK);
}

static void unmap_kernel_page(uintptr_t kernel_va)
{
    // do not create if missing
    uint64_t *pte = walk_kernel_page_table(kernel_va, false /* create */);
    if (pte && (*pte & PE_PRESENT)) {
        *pte = 0;
    }
}

static void copy_kernel_pml4_entries(uint64_t *dest, const uint64_t *src)
{
    // For a typical x86_64 design:
    //   Usually the kernel PML4 entries start around index 256 or so.
    //   This is just an example - adapt to your kernel layout.
    for (int i = 256; i < 512; i++) {
        dest[i] = src[i];  // shallow copy
    }
}

static void copy_user_pml4_entries(uint64_t *dest, const uint64_t *src, bool deepcopy_user)
{
    // If you're only doing a shallow copy for user space, just do:
    for (int i = 0; i < 256; i++) {
        // If user PDE present, you might re-use the same PDE pointer:
        // (If 'deepcopy_user' is false, we do a shallow copy.)
        // If 'deepcopy_user' is true, you might do something else,
        // or skip them entirely if you like. This example always does shallow:
        dest[i] = src[i];
    }
}

/*
 * fork_page_tables()
 *   Creates a brand-new PML4 page for the new address space,
 *   does a shallow copy of the old PML4’s kernel portion & user portion
 *   (without re‐mapping the old CR3 as a direct pointer).
 *
 *   - out_pages: optional pointer to a list of newly allocated "meta" pages
 *                (including the new top-level PML4).
 *   - deepcopy_user: if true, you might do a deeper user copy. This snippet
 *                    always does a shallow copy for illustration.
 *
 * Returns the physical address of the new top-level PML4.
 */
uintptr_t fork_page_tables(page_t **out_pages, bool deepcopy_user)
{
    kprintf("fork_page_tables: entered, deepcopy_user=%d\n", deepcopy_user);

    // We'll keep track of pages we allocate (the new PML4, possibly more)
    LIST_HEAD(page_t) table_pages;
    LIST_INIT(&table_pages);

    // 1) Allocate one physical page for the new PML4
    page_t *new_pml4_page = alloc_pages(1);
    if (!new_pml4_page) {
        panic("fork_page_tables: out of memory for new PML4");
    }
    SLIST_ADD(&table_pages, new_pml4_page, next);

    // This is the physical address of the new PML4
    uintptr_t new_pml4_phys = new_pml4_page->address;

    // 2) Zero out the new PML4 to avoid garbage entries
    {
        void *va_newpml4 = kmap_single_page(new_pml4_phys, PE_WRITE);
        memset(va_newpml4, 0, PAGE_SIZE);
        kunmap_single_page(va_newpml4);
    }

    // 3) Grab the old CR3 (which is the old PML4 physical address)
    uintptr_t old_cr3_phys = get_current_pgtable();
    kprintf("[DEBUG] old_cr3_phys = 0x%llx\n", (unsigned long long)old_cr3_phys);

    // 4) Make a local copy of the entire old PML4 into an array
    //    so we never have to read the old CR3 as a pointer again.
    uint64_t old_pml4_copy[512];
    {
        const void *va_oldpml4 = kmap_single_page(old_cr3_phys, PE_WRITE);
        memcpy(old_pml4_copy, va_oldpml4, PAGE_SIZE);
        kunmap_single_page(va_oldpml4);
    }
    // Now old_pml4_copy[] holds all 512 entries of the old top-level PML4.

    // 5) Map the new PML4 for writing, copy kernel & user entries
    {
        uint64_t *va_newpml4 = (uint64_t *)kmap_single_page(new_pml4_phys, PE_WRITE);

        // example: copy the kernel portion
        copy_kernel_pml4_entries(va_newpml4, old_pml4_copy);

        // example: shallow copy user portion
        copy_user_pml4_entries(va_newpml4, old_pml4_copy, deepcopy_user);

        // done writing new PML4
        kunmap_single_page(va_newpml4);
    }

    // 6) Optionally return the linked list of allocated pages
    if (out_pages) {
        *out_pages = LIST_FIRST(&table_pages);
    }

    // 7) Return the physical address of the new PML4
    return new_pml4_phys;
}
