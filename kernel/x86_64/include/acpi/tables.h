#ifndef ACPI_TABLES_H
#define ACPI_TABLES_H

#include <stdint.h>

#pragma pack(push, 1)

// Common ACPI System Description Table (SDT) header
typedef struct _acpi_sdt_hdr_t {
    char     signature[4];      // Signature (e.g., "RSDT", "XSDT", "FACP", etc.)
    uint32_t len;               // Length of the table, including this header
    uint8_t  rev;               // Revision of the table
    uint8_t  checksum;          // Entire table must sum to zero
    char     oem_id[6];         // OEM ID
    char     oem_table_id[8];   // OEM table identifier
    uint32_t oem_revision;      // OEM revision number
    uint32_t creator_id;        // Vendor ID of the utility that created the table
    uint32_t creator_revision;  // Revision of the utility that created the table
} _acpi_sdt_hdr_t;

// Root System Description Pointer (RSDP) structure (ACPI 1.0 and 2.0+)
typedef struct _acpi_rsdp_t {
    char     signature[8];      // "RSD PTR " (including space)
    uint8_t  checksum;          // Checksum of first 20 bytes for ACPI 1.0
    char     oem_id[6];         // OEM ID
    uint8_t  rev;               // Revision (0 = ACPI 1.0, 2 = ACPI 2.0+)
    uint32_t rsdt_ptr;          // 32-bit physical address of RSDT

    // ACPI 2.0+ extension fields:
    uint32_t length;            // Total length of the RSDP structure
    uint64_t xsdt_ptr;          // 64-bit physical address of the XSDT
    uint8_t  extended_checksum; // Checksum of entire table
    uint8_t  reserved[3];       // Reserved bytes (must be zero)
} _acpi_rsdp_t;

// Root System Description Table (RSDT) structure for ACPI 1.0
typedef struct _acpi_rsdt_t {
    _acpi_sdt_hdr_t hdr;        // Common header for all ACPI tables
    uint32_t tables[];          // Array of 32-bit physical addresses to other tables
} _acpi_rsdt_t;

// Extended System Description Table (XSDT) structure for ACPI 2.0+
typedef struct _acpi_xsdt_t {
    _acpi_sdt_hdr_t hdr;        // Common header for all ACPI tables
    uint64_t tables[];          // Array of 64-bit physical addresses to other tables
} _acpi_xsdt_t;

// Fixed ACPI Description Table (FADT/FACP)
// Minimal version: only fields required for ACPI enable/disable and PM registers are defined.
typedef struct _acpi_fadt_t {
    _acpi_sdt_hdr_t hdr;        // Common ACPI table header
    uint32_t facs;              // 32-bit physical address of the Firmware ACPI Control Structure (FACS)
    uint32_t dsdt;              // 32-bit physical address of the Differentiated System Description Table (DSDT)
    uint8_t  reserved;          // Reserved field (must be 0)
    uint8_t  preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint32_t acpi_enable;
    uint32_t acpi_disable;
    uint32_t s4bios_req;
    uint32_t pstate_cnt;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t  pm1_cnt_len;
    uint8_t  pm1_evt_len;
    uint8_t  pm2_cnt_len;
    uint8_t  pm_tmr_len;
    uint8_t  gpe0_blk_len;
    uint8_t  gpe1_blk_len;
    uint8_t  gpe1_base;
    uint8_t  cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;
    uint8_t  mon_alrm;
    uint8_t  century;
    uint16_t iapc_boot_arch;
    uint8_t  reserved2;
    uint32_t flags;
    uint64_t x_facs;            // 64-bit address of FACS (ACPI 2.0+)
    uint64_t x_dsdt;            // 64-bit address of DSDT (ACPI 2.0+)
    // Additional fields are defined in the full specification but are not needed here.
} _acpi_fadt_t;

// Multiple APIC Description Table (MADT) structure (contains APIC-related entries)
typedef struct acpi_madt_t {
    _acpi_sdt_hdr_t hdr;        // Common header
    uint32_t local_interrupt_controller_addr;
    uint32_t flags;
    // Followed by variable-length entries (MADT entries)
} acpi_madt_t;

// MADT entry header (each entry begins with a type and length)
typedef struct acpi_madt_entry_t {
    uint8_t type;
    uint8_t length;
} acpi_madt_entry_t;

// MADT Type 0: Processor Local APIC
typedef struct apic_local_t {
    uint8_t type;               // 0
    uint8_t length;
    uint8_t acpiProcessorId;
    uint8_t apicID;
    uint32_t flags;
} apic_local_t;

// MADT Type 1: I/O APIC
typedef struct apic_io_t {
    uint8_t type;               // 1
    uint8_t length;
    uint8_t id;
    uint8_t reserved;
    uint32_t address;
    uint32_t globalSystemInterruptBase;
} apic_io_t;

// MADT Type 2: Interrupt Source Override
typedef struct apic_interrupt_override_t {
    uint8_t type;               // 2
    uint8_t length;
    uint8_t busSource;
    uint8_t irqSource;
    uint32_t globalSystemInterrupt;
    uint16_t flags;
} apic_interrupt_override_t;

// MADT Type 4: Local APIC NMI
typedef struct apic_local_nmi_t {
    uint8_t type;               // 4
    uint8_t length;
    uint8_t processorID;        // 0xFF for all processors
    uint16_t flags;
    uint8_t lint_num;
} apic_local_nmi_t;

// MADT Type 5: Local APIC Address Override (for systems with a non-standard LAPIC address)
typedef struct apic_local_addr_override_t {
    uint8_t type;               // 5
    uint8_t length;
    uint64_t address;
} apic_local_addr_override_t;

#pragma pack(pop)

#endif // ACPI_TABLES_H
