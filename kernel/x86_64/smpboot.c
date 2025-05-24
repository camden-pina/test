#include <smpboot.h>
#include <interrupts/ioapic.h>
#include <interrupts/lapic.h>
#include <mm_types.h>
#include <printf.h>
#include <panic.h>
#include <string.h>
#include <mm/vmem.h>
#include <percpu.h>
#include <mm/pgtable.h>
#include <acpi/acpi.h>
#include <mm/pmm.h>
#include <mm/buddy.h>

#define SMPBOOT_START 0x1000
#define SMPDATA_START 0x2000

extern void smpboot_start(void);
extern void smpboot_end(void);

// incremented non-atomically by the bsp _only_
uint32_t system_num_cpus = 1;

#define MAX_LOCK_WAIT 1000000  // Maximum iterations to wait for the lock

int smp_boot_ap(uint32_t id, struct smp_data *smpdata) {
    kprintf("DEBUG: Entering smp_boot_ap for CPU#%d\n", id);

    /* Wait until AP acquires lock with timeout */
    kprintf("DEBUG: CPU#%d waiting for lock. Initial lock value: %d\n", id, smpdata->lock);
    int wait_counter = 0;
    while (!smpdata->lock) {
        if (wait_counter % 100000 == 0) {
            kprintf("DEBUG: CPU#%d waiting... (iteration %d, lock=%d)\n", id, wait_counter, smpdata->lock);
        }
        cpu_pause();
        wait_counter++;
        if (wait_counter > MAX_LOCK_WAIT) {
            kprintf("ERROR: CPU#%d timed out waiting for lock. Current lock value: %d\n", id, smpdata->lock);
            break;
        }
    }
    /* Extra pauses for stability */
    cpu_pause();
    cpu_pause();

    kprintf("smp: booting CPU#%d\n", id);

    /* Allocate per-cpu area for AP */
    kprintf("DEBUG: CPU#%d: Allocating percpu area\n", id);
    struct percpu *percpu_area = percpu_alloc_area(id);
    if (!percpu_area) {
        kprintf("ERROR: CPU#%d: Failed to allocate percpu area!\n", id);
    } else {
        kprintf("DEBUG: CPU#%d: Percpu area allocated at %p\n", id, percpu_area);
    }

    /* Allocate stack pages */
    kprintf("DEBUG: CPU#%d: Allocating stack pages for size %d\n", id, KERNEL_STACK_SIZE);
    page_t *stack_pages = alloc_pages(SIZE_TO_PAGES(KERNEL_STACK_SIZE));
    if (!stack_pages) {
        kprintf("ERROR: CPU#%d: Failed to allocate stack pages!\n", id);
    } else {
        kprintf("DEBUG: CPU#%d: Stack pages allocated\n", id);
    }

    /* Map stack pages to virtual memory */
    kprintf("DEBUG: CPU#%d: Mapping stack pages into virtual memory\n", id);
    kprintf("stack pages: %llx\n", stack_pages);
    extern address_space_t *kernel_space;
    extern address_space_t *default_user_space;
    #define AP_STACK_BASE 0xFFFF800000800000ULL

uintptr_t ap_stack_hint = AP_STACK_BASE + (KERNEL_STACK_SIZE * id);
void *ap_stack_ptr = (void *) vmap_pages(moveref(stack_pages), ap_stack_hint, KERNEL_STACK_SIZE, VM_READ | VM_WRITE | VM_STACK | VM_EXEC, "ap stack");
    vm_set_current_space(kernel_space);
vm_print_address_space();
    kprintf("new cpu stack: %llx\n", ap_stack_ptr);

    // void *ap_stack_ptr = (void *) vmap_pages(moveref(stack_pages), 0, KERNEL_STACK_SIZE, VM_WRITE | VM_STACK, "ap stack");
    if (!ap_stack_ptr) {
        kprintf("ERROR: CPU#%d: Failed to map AP stack!\n", id);
    } else {
        kprintf("DEBUG: CPU#%d: AP stack mapped at %p\n", id, ap_stack_ptr);
    }

    /* Get and set the default AP PML4 address */
    uint32_t ap_pml4 = (uint32_t) kernel_space->page_table;
    kprintf("DEBUG: CPU#%d: Retrieved default AP PML4 address: 0x%x\n", id, ap_pml4);
    smpdata->pml4_addr = (uint32_t) get_default_ap_pml4(); // ap_pml4;
    smpdata->percpu_ptr = (uintptr_t) percpu_area;
    smpdata->stack_addr = (uintptr_t) ap_stack_ptr + KERNEL_STACK_SIZE;

    kprintf("DEBUG: CPU#%d: Set smpdata->pml4_addr=0x%x, percpu_ptr=0x%lx, stack_addr=0x%lx\n",
            id, smpdata->pml4_addr, smpdata->percpu_ptr, smpdata->stack_addr);

    /* Release gate to signal readiness */
    kprintf("DEBUG: CPU#%d: Releasing gate (setting gate to 0)\n", id);
    smpdata->gate = 0;

    /* Wait until the AP finishes its initialization (gate set to non-zero) */
    kprintf("DEBUG: CPU#%d: Waiting for AP to finish its initialization (gate will be set to non-zero)\n", id);
    while (!smpdata->gate) {
        kprintf("DEBUG: CPU#%d: Still waiting for AP gate signal...\n", id);
        cpu_pause();
    }

    kprintf("smp: booted CPU#%d!\n", id);
    kprintf("DEBUG: CPU#%d: Resetting smpdata pointers and addresses\n", id);
    smpdata->pml4_addr = 0;
    smpdata->percpu_ptr = 0;
    smpdata->stack_addr = 0;

    kprintf("DEBUG: Exiting smp_boot_ap for CPU#%d\n", id);
    return 0;
}

void smp_init() {
    kprintf("DEBUG: Entering smp_init\n");

    if (!is_smp_enabled) {
        kprintf("smp: disabled\n");
        system_num_cpus = 1;
        kprintf("DEBUG: SMP disabled; system_num_cpus set to 1\n");
        return;
    }

    void *code_ptr;
    {
        kprintf("DEBUG: Allocating code page at SMPBOOT_START (0x%x)\n", SMPBOOT_START);
        page_t *code_page = alloc_pages_at(SMPBOOT_START, 1, PAGE_SIZE);
        if (!code_page) {
            kprintf("ERROR: Failed to allocate code page at SMPBOOT_START\n");
            return;
        }
        kassert(code_page != NULL);
        code_ptr = (void *)vmap_pages(moveref(code_page), 0, PAGE_SIZE, VM_WRITE | VM_EXEC, "smpboot code");
        if (!code_ptr) {
            kprintf("ERROR: Failed to map code page to virtual memory!\n");
        }
        kassert(code_ptr != NULL);
        kprintf("DEBUG: Code page allocated and mapped at virtual address: 0x%lx\n", (uintptr_t)code_ptr);
    }

    void *data_ptr;
    {
        kprintf("DEBUG: Allocating data page at SMPDATA_START (0x%x)\n", SMPDATA_START);
        page_t *data_page = alloc_pages_at(SMPDATA_START, 1, PAGE_SIZE);
        if (!data_page) {
            kprintf("ERROR: Failed to allocate data page at SMPDATA_START\n");
            return;
        }
        kassert(data_page != NULL);
        data_ptr = (void *)vmap_pages(moveref(data_page), 0, PAGE_SIZE, VM_WRITE | VM_NOCACHE | VM_EXEC, "smpboot data");
        if (!data_ptr) {
            kprintf("ERROR: Failed to map data page to virtual memory!\n");
        }
        kassert(data_ptr != NULL);
        kprintf("DEBUG: Data page allocated and mapped at virtual address: 0x%lx\n", (uintptr_t)data_ptr);
    }

    uintptr_t eip = virt_to_phys(code_ptr);
    kprintf("DEBUG: eip (physical address of code_ptr) calculated as: 0x%lx\n", eip);

    size_t smpboot_size = (uintptr_t)(void *)&smpboot_end - (uintptr_t)(void *)&smpboot_start;
    kprintf("DEBUG: smpboot code size: %zu bytes\n", smpboot_size);
    kassert(smpboot_size <= PAGE_SIZE); // we need this kassert, bring it back later
    kassert(sizeof(struct smp_data) <= PAGE_SIZE);

    kprintf("DEBUG: Copying smpboot code from %p to code_ptr (%p)\n", smpboot_start, code_ptr);
    memcpy(code_ptr, smpboot_start, 1744); // 1744
    kprintf("DEBUG: Zeroing the data page (PAGE_SIZE = %d bytes)\n", PAGE_SIZE);
    memset(data_ptr, 0, PAGE_SIZE);

    struct smp_data *smpdata = data_ptr;
    kprintf("DEBUG: Initializing smpdata: setting lock=0 and gate=1\n");
    smpdata->lock = 0;
    smpdata->gate = 1;

    /* Issue INIT-SIPI-SIPI Sequence to start-up all APs */
    kprintf("DEBUG: Issuing INIT to APIC\n");
    apic_write_icr(APIC_DM_INIT | APIC_LVL_ASSERT | APIC_DS_ALLBUT, 0);
    apic_mdelay(10);

    kprintf("DEBUG: Issuing first SIPI to APIC with vector 0x%lx\n", (unsigned long)(eip >> 12));
    apic_write_icr(APIC_DM_STARTUP | APIC_LVL_ASSERT | APIC_DS_ALLBUT | (eip >> 12), 0);
    apic_udelay(200);

    kprintf("DEBUG: Issuing second SIPI to APIC with vector 0x%lx\n", (unsigned long)(eip >> 12));
    apic_write_icr(APIC_DM_STARTUP | APIC_LVL_ASSERT | APIC_DS_ALLBUT | (eip >> 12), 0);
    apic_udelay(200);

    uint32_t timeout_us = 100;
    kprintf("DEBUG: Waiting for APs to update count (expected: %d APs)\n", total_apic_count - 1);
    while (timeout_us > 0 && smpdata->count < total_apic_count - 1) {
        cpu_pause();
        apic_udelay(1);
        timeout_us--;
        if (timeout_us % 10 == 0) {
            kprintf("DEBUG: Timeout loop update, timeout_us=%d, current AP count=%d\n", timeout_us, smpdata->count);
        }
    }
    if (smpdata->count < total_apic_count - 1) {
        kprintf("ERROR: Timeout waiting for APs. Expected %d APs, but got %d\n", total_apic_count - 1, smpdata->count);
    } else {
        kprintf("DEBUG: AP count reached: %d\n", smpdata->count);
    }

    kprintf("DEBUG: Booting APs. Looping over %d AP(s)\n", smpdata->count);
    for (int i = 0; i < smpdata->count; i++) {
        uint16_t id = i + 1;
        kprintf("DEBUG: Booting AP with CPU id: %d (iteration %d)\n", id, i);
        smp_boot_ap(id, smpdata);
        system_num_cpus++;
        kprintf("DEBUG: system_num_cpus incremented to: %d\n", system_num_cpus);
    }

    kprintf("DEBUG: Freeing code and data mappings\n");
    vmap_free((uintptr_t) code_ptr, PAGE_SIZE);
    vmap_free((uintptr_t) data_ptr, PAGE_SIZE);

    kprintf("smp: total cpus = %d\n", system_num_cpus);
    kprintf("smp: done!\n");
    kprintf("DEBUG: Exiting smp_init\n");
}
