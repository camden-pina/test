// acpi.c
// ##############################
//         ACPI Initialization and Host Interface
// ##############################

#include <acpi/acpi.h>
#include <node.h>
#include <stdint.h>
#include <string.h>
#include <acpi/tables.h>
#include <printf.h>
#include <panic.h>
#include <lai/core.h>
#include <mm/pmm.h>

// Global ACPI table pointers
struct _acpi_xsdt_t* xsdt = NULL;
struct _acpi_fadt_t* fadt = NULL;

// ACPI variables (for power management, etc.)
static unsigned int  acpi_smi_cmd;
static unsigned char acpi_enable;
static unsigned char acpi_disable;
static unsigned int  acpi_pm1a_cnt;
static unsigned int  acpi_pm1b_cnt;
static unsigned short acpi_slp_typ_a;
static unsigned short acpi_slp_typ_b;
static unsigned short acpi_slp_en;
static unsigned short acpi_sci_en;
static unsigned char  acpi_pm1_cnt_len;

// Calculate checksum for an ACPI table.
// Returns nonzero (true) if the table's bytes sum to zero.
unsigned int acpi_checksum(void* ptr) {
    _acpi_sdt_hdr_t* hdr = (_acpi_sdt_hdr_t*)ptr;
    unsigned char* bytes = (unsigned char*)ptr;
    unsigned char sum = 0;
    for (unsigned int i = 0; i < hdr->len; i++) {
        sum += bytes[i];
    }
    return (sum == 0);
}

// Locate an ACPI table by signature within the XSDT.
void* acpi_locate_table(const char* table) {
    if (!xsdt)
        return NULL;
    unsigned long long xsdt_entries = (xsdt->hdr.len - sizeof(xsdt->hdr)) / sizeof(uint64_t);
    for (unsigned long long e = 0; e < xsdt_entries; e++) {
        _acpi_sdt_hdr_t* hdr = (_acpi_sdt_hdr_t*)(uintptr_t)(xsdt->tables[e]);
        if (memcmp(hdr->signature, table, 4) == 0)
            return (void*)hdr;
    }
    return NULL;
}

// Initialize ACPI using the provided RSDP pointer.
_Bool acpi_init(unsigned long long* rsdp_ptr) {
    acpi_smi_cmd     = 0;
    acpi_enable      = 0;
    acpi_disable     = 0;
    acpi_pm1a_cnt    = 0;
    acpi_pm1b_cnt    = 0;
    acpi_slp_typ_a   = 0;
    acpi_slp_typ_b   = 0;
    acpi_slp_en      = 0;
    acpi_sci_en      = 0;
    acpi_pm1_cnt_len = 0;

    xsdt = NULL;
    fadt = NULL;

    kprintf("Initializing ACPI...\n\r");

    _acpi_rsdp_t* rsdp = (_acpi_rsdp_t*)rsdp_ptr;
    xsdt = (_acpi_xsdt_t*)(uintptr_t)(rsdp->xsdt_ptr);

    kprintf("Verifying XSDT checksum...\n\r");
    if (!acpi_checksum(xsdt))
        PANIC("ACPI Init: Invalid XSDT Checksum");

    kprintf("Locating FADT...\n\r");
    fadt = (_acpi_fadt_t*)acpi_locate_table("FACP");
    if (!fadt)
        PANIC("ACPI Init: FADT not found");

    kprintf("Verifying FADT checksum...\n\r");
    if (!acpi_checksum(fadt))
        PANIC("ACPI Init: Invalid FADT Checksum");

    // Retrieve DSDT pointer from FADT (prefer extended address if available)
    void* dsdt_ptr = (fadt->x_dsdt) ? (void*)(uintptr_t)fadt->x_dsdt : (void*)(uintptr_t)fadt->dsdt;
    kprintf("Verifying DSDT checksum...\n\r");
    if (!dsdt_ptr || !acpi_checksum(dsdt_ptr))
        PANIC("ACPI Init: Invalid DSDT Checksum");

    // Initialize the LAI library for AML interpretation.
    lai_set_acpi_revision(2.0);
    lai_create_namespace();

    kprintf("ACPI initialization complete.\n\r");
    return 1;
}

// =====================================================
// LAI Host Functions
// (These functions allow the LAI library to interface with your OS.)
// They are currently stubs and should be filled with real implementations.
// =====================================================

void *laihost_malloc(size_t sz) {
    return kmalloc(sz);
}

void *laihost_realloc(void *ptr, size_t newsize, size_t oldsize) {
    return krealloc(ptr, newsize);
}

void laihost_free(void *ptr, size_t sz) {
    kfree(ptr);
}

void laihost_log(int level, const char *message) {
    kprintf("[%d] %s", level, message);
}

void laihost_panic(const char *message) {
    panic("%s", message);
}

void *laihost_scan(char *signature, size_t index) {
    kprintf("laihost_scan() [%s], [%llu]\n", signature, index);
    // Special-case: DSDT must be obtained from the FADT.
    if (memcmp(signature, "DSDT", 4) == 0) {
        if (!fadt) {
            kprintf("laihost_scan: FADT not found, cannot locate DSDT.\n");
            return NULL;
        }
        void* dsdt_ptr = (fadt->x_dsdt) ? (void*)(uintptr_t)fadt->x_dsdt : (void*)(uintptr_t)fadt->dsdt;
        if (dsdt_ptr && acpi_checksum(dsdt_ptr)) {
            kprintf("laihost_scan: DSDT found.\n");
            return dsdt_ptr;
        } else {
            kprintf("laihost_scan: DSDT checksum failed or not found.\n");
            return NULL;
        }
    }
    void* ptr = NULL;
    acpi_status_t status = acpi_table_find(&ptr, signature, index);
    if (status == ACPI_SUCCESS) {
        kprintf("laihost_scan: ACPI table %4.4s found at index %llu\n", signature, index);
        return ptr;
    } else {
        kprintf("laihost_scan: Failed to find ACPI table %4.4s at index %llu\n", signature, index);
        return NULL;
    }
}

void *laihost_map(size_t address, size_t size) {
    // TODO: Implement memory mapping functionality if required.
    return NULL;
}

void laihost_unmap(void *addr, size_t size) {
    // TODO: Implement memory unmapping functionality if required.
}

void laihost_outb(uint16_t port, uint8_t value) {
    // TODO: Implement port I/O (byte output).
}

void laihost_outw(uint16_t port, uint16_t value) {
    // TODO: Implement port I/O (word output).
}

void laihost_outd(uint16_t port, uint32_t value) {
    // TODO: Implement port I/O (double-word output).
}

uint8_t laihost_inb(uint16_t port) {
    // TODO: Implement port I/O (byte input).
    return 0;
}

uint16_t laihost_inw(uint16_t port) {
    // TODO: Implement port I/O (word input).
    return 0;
}

uint32_t laihost_ind(uint16_t port) {
    // TODO: Implement port I/O (double-word input).
    return 0;
}

void laihost_pci_writeb(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function,
                        uint16_t offset, uint8_t value) {
    // TODO: Implement PCI byte write.
}

uint8_t laihost_pci_readb(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function,
                          uint16_t offset) {
    // TODO: Implement PCI byte read.
    return 0;
}

void laihost_pci_writew(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function,
                        uint16_t offset, uint16_t value) {
    // TODO: Implement PCI word write.
}

uint16_t laihost_pci_readw(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function,
                           uint16_t offset) {
    // TODO: Implement PCI word read.
    return 0;
}

void laihost_pci_writed(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function,
                        uint16_t offset, uint32_t value) {
    // TODO: Implement PCI double-word write.
}

uint32_t laihost_pci_readd(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function,
                           uint16_t offset) {
    // TODO: Implement PCI double-word read.
    return 0;
}

void laihost_sleep(uint64_t duration) {
    // TODO: Implement sleep functionality.
}

uint64_t laihost_timer(void) {
    return 0;
}

int laihost_sync_wait(struct lai_sync_state *state, unsigned int val, int64_t deadline) {
    return 0;
}

void laihost_sync_wake(struct lai_sync_state *state) {
    // TODO: Implement synchronization wake.
}

void laihost_handle_amldebug(lai_variable_t *variable) {
    // TODO: Implement AML debugging functionality.
}

void laihost_handle_global_notify(lai_nsnode_t *node, int event) {
    // TODO: Implement global notify handling.
}
