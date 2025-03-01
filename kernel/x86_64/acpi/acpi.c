// ACPI initialization – supports ACPI 1.0 (RSDT) and 2.0+ (XSDT)
#include <acpi/acpi.h>
#include <acpi/tables.h>
#include <lai/core.h>
#include <io.h>
#include <printf.h>
#include <panic.h>
#include <string.h>
#include <mm/pmm.h>

/**
 * @brief io_wait - Wait for an I/O operation to complete.
 *
 * This function writes to the unused I/O port 0x80.
 * It is traditionally used to introduce a small delay
 * ensuring that previous I/O operations have time to settle.
 */
void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

/**
 * @brief __rdtsc - Read the CPU's Time Stamp Counter.
 *
 * This function returns the 64-bit value of the Time Stamp Counter (TSC),
 * which increments every clock cycle. This can be used for high-resolution
 * timing measurements.
 *
 * @return uint64_t The current TSC value.
 */
uint64_t __rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Global ACPI table pointers
struct _acpi_xsdt_t *xsdt = NULL;
struct _acpi_rsdt_t *rsdt = NULL;         // Added: RSDT support for ACPI 1.0
struct _acpi_fadt_t *fadt = NULL;

// Static ACPI configuration values
static uint16_t acpi_pm1a_cnt = 0, acpi_pm1b_cnt = 0;
static uint16_t acpi_sci_en_bit = 0;      // Bit mask for SCI_EN in PM1 control
static uint16_t acpi_slp_en_bit = 0;      // Bit mask for SLP_EN in PM1 control
static uint8_t  acpi_smi_cmd = 0;
static uint8_t  acpi_enable_val = 0, acpi_disable_val = 0;
static uint8_t  acpi_pm1_cnt_len = 0;

// Compute checksum of an ACPI table (returns 1 if valid)
unsigned int acpi_checksum(void *table) {
    _acpi_sdt_hdr_t *hdr = (_acpi_sdt_hdr_t *)table;
    uint8_t *bytes = (uint8_t *)table;
    uint8_t sum = 0;
    for (unsigned int i = 0; i < hdr->len; ++i) {
        sum += bytes[i];
    }
    return (sum == 0);  // ACPI checksum is valid if sum of bytes == 0
}

// Locate a table by signature in XSDT or RSDT
void* acpi_find_table(const char *sig, size_t index) {
    if (!(xsdt || rsdt)) return NULL;  // No system table loaded
    size_t count, found = 0;
    if (xsdt) {
        // Use 64-bit XSDT entries
        count = (xsdt->hdr.len - sizeof(xsdt->hdr)) / sizeof(uint64_t);
        for (size_t i = 0; i < count; ++i) {
            _acpi_sdt_hdr_t *hdr = (_acpi_sdt_hdr_t *)(uintptr_t) xsdt->tables[i];
            if (hdr && memcmp(hdr->signature, sig, 4) == 0) {
                if (found == index) return (void*)hdr;
                ++found;
            }
        }
    } else if (rsdt) {
        // Use 32-bit RSDT entries
        count = (rsdt->hdr.len - sizeof(rsdt->hdr)) / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            _acpi_sdt_hdr_t *hdr = (_acpi_sdt_hdr_t *)(uintptr_t) rsdt->tables[i];
            if (hdr && memcmp(hdr->signature, sig, 4) == 0) {
                if (found == index) return (void*)hdr;
                ++found;
            }
        }
    }
    return NULL;  // Not found
}

_Bool acpi_init(uint64_t rsdp_addr) {
    // Interpret the RSDP from boot loader
    _acpi_rsdp_t *rsdp = (_acpi_rsdp_t *)(uintptr_t) rsdp_addr;
    if (!rsdp || memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        PANIC("ACPI: RSDP not found or invalid");
    }
    kprintf("ACPI: RSDP revision %d detected\n", rsdp->rev);
    
    // Use XSDT for ACPI 2.0+, else RSDT for ACPI 1.0
    if (rsdp->rev >= 2 && rsdp->xsdt_ptr != 0) {
        xsdt = (_acpi_xsdt_t *)(uintptr_t) rsdp->xsdt_ptr;
        if (!acpi_checksum(xsdt)) {
            PANIC("ACPI: XSDT checksum invalid");
        }
        kprintf("ACPI: XSDT located at %p, entries=%llu\n", xsdt,
                (xsdt->hdr.len - sizeof(xsdt->hdr)) / sizeof(uint64_t));
    } else {
        rsdt = (struct _acpi_rsdt_t *)(uintptr_t) rsdp->rsdt_ptr;
        if (!rsdt || !acpi_checksum(rsdt)) {
            PANIC("ACPI: RSDT not found or checksum invalid");
        }
        kprintf("ACPI: RSDT located at %p, entries=%u\n", rsdt,
                (unsigned)((rsdt->hdr.len - sizeof(rsdt->hdr)) / sizeof(uint32_t)));
    }

    // Find and validate the Fixed ACPI Description Table (FADT, signature "FACP")
    fadt = (_acpi_fadt_t *) acpi_find_table("FACP", 0);
    if (!fadt || !acpi_checksum(fadt)) {
        PANIC("ACPI: FADT not found or corrupted");
    }
    kprintf("ACPI: FADT found at %p\n", fadt);

    // Locate the Differentiated System Description Table (DSDT) via FADT
    void *dsdt_ptr = (fadt->x_dsdt) 
                     ? (void*)(uintptr_t) fadt->x_dsdt 
                     : (void*)(uintptr_t) fadt->dsdt;

    if (!dsdt_ptr || !acpi_checksum(dsdt_ptr)) {
        PANIC("ACPI: DSDT not found or checksum invalid");
    }
    kprintf("ACPI: DSDT found at %p\n", dsdt_ptr);

    // ** Enable ACPI mode if not already active **
    acpi_smi_cmd    = fadt->smi_cmd;
    acpi_enable_val = fadt->acpi_enable;
    acpi_disable_val= fadt->acpi_disable;
    acpi_pm1a_cnt   = fadt->pm1a_cnt_blk;
    acpi_pm1b_cnt   = fadt->pm1b_cnt_blk;
    acpi_pm1_cnt_len= fadt->pm1_cnt_len;
    if (acpi_pm1_cnt_len > 0) {
        // ACPI SCI_EN is usually bit 0 in PM1 control register
        acpi_sci_en_bit = 1 << 0;
        // Sleep enable bit (SLP_EN) is bit 13 as per ACPI spec
        acpi_slp_en_bit = 1 << 13;
    }
    if (acpi_smi_cmd != 0 && acpi_enable_val != 0) {
        // Read current ACPI enable status from PM1a_CNT (and PM1b if exists)
        uint16_t pm1a_status = inw(acpi_pm1a_cnt);
        uint16_t pm1b_status = acpi_pm1b_cnt ? inw(acpi_pm1b_cnt) : 0;
        if (!(pm1a_status & acpi_sci_en_bit) && !(pm1b_status & acpi_sci_en_bit)) {
            kprintf("ACPI: Enabling ACPI mode...\n");
            outb(acpi_smi_cmd, acpi_enable_val);  // send ACPI enable command
            // Wait until SCI_EN is set to confirm ACPI mode (simple timeout loop)
            for (int i = 0; i < 300; ++i) {
                pm1a_status = inw(acpi_pm1a_cnt);
                if (pm1a_status & acpi_sci_en_bit) break;
                io_wait();  // small delay (io_wait is a port I/O delay or similar)
            }
            kprintf("ACPI: ACPI mode %s.\n", 
                    (pm1a_status & acpi_sci_en_bit) ? "enabled" : "not enabled!");
        }
    }

    // Initialize ACPI AML interpreter (LAI library)
    lai_set_acpi_revision(fadt->hdr.rev);   // Set ACPI revision for AML interpreter
    lai_create_namespace();                // Parse ACPI tables (DSDT/SSDT) and create ACPI namespace
    kprintf("ACPI: Namespace initialized via LAI\n");

    kprintf("ACPI initialization complete.\n");
    return 1;
}

// ACPI table scan function used by LAI (and internally) to find tables by signature.
void *acpi_scan(const char *signature, size_t index) {
    // Special-case DSDT: obtained from FADT rather than XSDT/RSDT
    if (memcmp(signature, "DSDT", 4) == 0) {
        if (!fadt) return NULL;
        void *dsdt_ptr = (fadt->x_dsdt) ? (void*)(uintptr_t)fadt->x_dsdt 
                                       : (void*)(uintptr_t)fadt->dsdt;
        return (dsdt_ptr && acpi_checksum(dsdt_ptr)) ? dsdt_ptr : NULL;
    }
    // Generic case: find table in XSDT/RSDT
    return acpi_find_table(signature, index);
}

// LAI (ACPI interpreter) host function implementations for our OS:
void *laihost_malloc(size_t size)        { return kmalloc(size); }   // use kernel heap allocator
void *laihost_realloc(void *ptr, size_t newsize, size_t oldsize) { return krealloc(ptr, newsize); }
void  laihost_free(void *ptr, size_t size) { kfree(ptr); }

void  laihost_log(int level, const char *msg)    { kprintf("[ACPI%llu] %s", level, msg); }
void  laihost_panic(const char *msg)             { panic("ACPI: %s", msg); }

void *laihost_scan(char *signature, size_t index) {
	return acpi_scan(signature, index);
}

// Map physical memory for ACPI access – in our simple setup, identity mapping is in effect
void *laihost_map(size_t phys_addr, size_t size) {
    // If using paging with no identity map, map pages here. For now, assume identity map:
    return (void*)(uintptr_t)phys_addr;
}
void laihost_unmap(void *virt_addr, size_t size) {
    // In identity-mapped scenario, no action needed. Otherwise, unmap pages if allocated.
    (void)virt_addr; (void)size;
}

// Port I/O operations for ACPI (e.g., accessing PM registers, CMOS, etc.)
void laihost_outb(uint16_t port, uint8_t value)   { outb(port, value); }
void laihost_outw(uint16_t port, uint16_t value)  { outw(port, value); }
void laihost_outd(uint16_t port, uint32_t value)  { outdw(port, value); }
uint8_t  laihost_inb(uint16_t port)  { return inb(port); }
uint16_t laihost_inw(uint16_t port)  { return inw(port); }
uint32_t laihost_ind(uint16_t port)  { return indw(port); }

// PCI configuration space access (using I/O ports 0xCF8/0xCFC for this platform)
#define PCI_CFG_ADDR_PORT 0xCF8
#define PCI_CFG_DATA_PORT 0xCFC
static inline uint32_t pci_config_address(uint16_t seg, uint8_t bus,
                                         uint8_t slot, uint8_t func, uint16_t offset) {
    // Construct 32-bit config address: enable bit (31), segments not used in legacy mechanism
    uint32_t address = (uint32_t)(bus << 16) | (uint32_t)(slot << 11) 
                     | (uint32_t)(func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000);
    return address;
}
void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func,
                        uint16_t offset, uint8_t value) {
    outdw(PCI_CFG_ADDR_PORT, pci_config_address(seg, bus, slot, func, offset));
    outb(PCI_CFG_DATA_PORT + (offset & 3), value);
}
uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func,
                          uint16_t offset) {
    outdw(PCI_CFG_ADDR_PORT, pci_config_address(seg, bus, slot, func, offset));
    return inb(PCI_CFG_DATA_PORT + (offset & 3));
}
void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func,
                        uint16_t offset, uint16_t value) {
    outdw(PCI_CFG_ADDR_PORT, pci_config_address(seg, bus, slot, func, offset));
    outw(PCI_CFG_DATA_PORT + (offset & 2), value);
}
uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func,
                           uint16_t offset) {
    outdw(PCI_CFG_ADDR_PORT, pci_config_address(seg, bus, slot, func, offset));
    return inw(PCI_CFG_DATA_PORT + (offset & 2));
}
void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func,
                        uint16_t offset, uint32_t value) {
    outdw(PCI_CFG_ADDR_PORT, pci_config_address(seg, bus, slot, func, offset));
    outdw(PCI_CFG_DATA_PORT, value);
}
uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func,
                           uint16_t offset) {
    outdw(PCI_CFG_ADDR_PORT, pci_config_address(seg, bus, slot, func, offset));
    return indw(PCI_CFG_DATA_PORT);
}

// Stub or simple implementations for remaining LAI hooks
void laihost_sleep(uint64_t ms) {
    // Simple delay loop or HPET/PIT wait can be implemented; stub for now
    uint64_t end = laihost_timer() + ms;
    while (laihost_timer() < end) { /* spin-wait */ }
}
uint64_t laihost_timer(void) {
    // Use Time Stamp Counter as a monotonic timer (assuming constant TSC)
    return __rdtsc();  // if rdtsc intrinsic available; else use a platform timer
}
int laihost_sync_wait(struct lai_sync_state *state, unsigned int value, int64_t deadline) {
    // No multi-threading in early boot, so just return immediately or simulate wait
    (void)state; (void)value; (void)deadline;
    return 0;
}
void laihost_sync_wake(struct lai_sync_state *state) {
    (void)state;
    // In a real OS, wake waiting threads; not needed in single-threaded init
}
void laihost_handle_amldebug(lai_variable_t *var) {
    // Print AML debugging information (if needed)
    if (!var) return;
    char buf[64];
    // lai_stringify_variable(buf, sizeof(buf), var);
    kprintf("AML Debug: %s\n", "LAI STRINGIFY VARIABLE()");
}
void laihost_handle_global_notify(lai_nsnode_t *node, int event) {
    // Respond to global notify events if required (not used in this setup)
    (void)node; (void)event;
}
