#include <mm/vmem.h>
#include <string.h>

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
        PML4 = (struct page_table*)bitmap_page_request();
        memset(PML4, 0, PAGE_SIZE);
        
        for (uint64_t idx = 0; idx < total_memory; idx += PAGE_SIZE)    // Identity map all pages
                vmem_memory_map((void*)idx, (void*)idx);
        
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