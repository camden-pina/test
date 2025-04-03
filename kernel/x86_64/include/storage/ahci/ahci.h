#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stddef.h>
#include <drivers/pci.h>        // Kernel's PCI helper (for pci_device_t, etc.)
#include <storage/block_device.h>      // Kernel block device abstractions
#include <interrupts/lapic.h>  // For registering IRQ handlers, if available
#include <mm/vmem.h>
#include <spinlock.h>
#include <mutex.h>

/*
 * Basic PCI class, subclass, and programming interface codes for AHCI
 */
#define PCI_CLASS_MASS_STORAGE   0x01
#define PCI_SUBCLASS_SATA        0x06
#define PCI_PROGIF_AHCI          0x01  // AHCI interface

/*
 * AHCI register definitions. In pure C, we cannot nest a struct definition
 * inside another and refer to it as struct HBA_MEM::HBA_PORT, so we define them
 * as separate top-level structs (hba_port_t, hba_mem_t).
 */

typedef volatile struct hba_port {
    uint32_t clb;    // 0x00
    uint32_t clbu;   // 0x04
    uint32_t fb;     // 0x08
    uint32_t fbu;    // 0x0C
    uint32_t is;     // 0x10
    uint32_t ie;     // 0x14
    uint32_t cmd;    // 0x18
    uint32_t rsv0;   // 0x1C
    uint32_t tfd;    // 0x20
    uint32_t sig;    // 0x24
    uint32_t ssts;   // 0x28
    uint32_t sctl;   // 0x2C
    uint32_t serr;   // 0x30
    uint32_t sact;   // 0x34
    uint32_t ci;     // 0x38
    uint32_t sntf;   // 0x3C
    uint32_t fbs;    // 0x40
    uint32_t rsv1[11];    // 0x44 ~ 0x6F
    uint32_t vendor[4];   // 0x70 ~ 0x7F
} hba_port_t;

typedef volatile struct hba_mem {
    // 0x00 ~ 0x2B
    uint32_t cap;    
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  reserved[0xA0-0x2C]; 
    uint8_t  vendor[0x100-0xA0];
    // 0x100 ~ 0x10FF: up to 32 ports, each 0x80 bytes
    hba_port_t ports[32];
} hba_mem_t;

// HBA port bits
#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000
#define HBA_GHC_HR      0x0001
#define HBA_GHC_IE      0x0002
#define HBA_GHC_AE      0x80000000
#define HBA_PxIS_TFES   (1 << 30)

// SATA signature values
#define SATA_SIG_ATA    0x00000101
#define SATA_SIG_ATAPI  0xEB140101
#define SATA_SIG_SEMB   0xC33C0101
#define SATA_SIG_PM     0x96690101

typedef enum {
    AHCI_DEV_NONE = 0,
    AHCI_DEV_SATA,
    AHCI_DEV_SATAPI,
    AHCI_DEV_SEMB,
    AHCI_DEV_PM
} ahci_dev_type_t;

// FIS definitions
#define FIS_TYPE_REG_H2D  0x27
#define FIS_TYPE_REG_D2H  0x34

typedef struct fis_reg_h2d {
    uint8_t  fis_type;    // 0x27
    uint8_t  pmport:4;
    uint8_t  rsv0:3;
    uint8_t  c:1;
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  rsv1[4];
} fis_reg_h2d_t;

// Command structures
typedef struct hba_cmd_header {
    uint8_t  cfl:5;     // Command FIS length in DWORDS
    uint8_t  atapi:1;
    uint8_t  write:1;
    uint8_t  prefetch:1;

    uint8_t  reset:1;
    uint8_t  bist:1;
    uint8_t  clear:1;
    uint8_t  rsv0:1;
    uint8_t  pmp:4;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} hba_cmd_header_t;

typedef struct hba_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved0;
    uint32_t dbc:22;
    uint32_t reserved1:9;
    uint32_t i:1;
} hba_prdt_entry_t;

typedef struct hba_cmd_tbl {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  reserved[48];
    hba_prdt_entry_t prdt_entry[];
} hba_cmd_tbl_t;

/*
 * Forward declarations
 */

// Forward-declare our structures
struct ahci_controller;
struct ahci_port;

// Each AHCI port has local state
typedef struct ahci_port {
    struct ahci_controller *hba; // back-pointer to the controller
    volatile hba_port_t *regs;   // pointer to the port registers
    ahci_dev_type_t dev_type;

    // Disk geometry
    uint64_t sector_count;
    uint32_t sector_size;

    // Memory for port structures
    uint8_t  *clb_virtual;  // command list base virtual
    uint8_t  *fis_virtual;  // received FIS base
    hba_cmd_tbl_t **cmd_tables;
    uint64_t clb_phys;
    uint64_t fis_phys;
    uint64_t *ctba_phys;

    // Active command slots bitmask
    uint32_t active_slots;

    spinlock_t lock;            // concurrency lock
    block_device_t *bdev;    // embedded block device
} ahci_port_t;

// The AHCI controller
typedef struct ahci_controller {
    volatile hba_mem_t *abar; // mapped ABAR
    pci_device_t *pci_dev;    // associated PCI device
    uint8_t irq;
    uint32_t port_count;
    ahci_port_t *ports;       // dynamic array of port info
} ahci_controller_t;

/*
 * Public API
 */
void ahci_init(void);                   // Initialize driver (find controllers, etc.)
void ahci_discover_controllers(void);   // Enumerate all PCI devices, find AHCI, init

// Called by kernel interrupt dispatch
void ahci_irq_handler(void *context);

// The block device read/write routines
int ahci_read(block_device_t *bdev, uint64_t lba, uint32_t count, void *buffer);
int ahci_write(block_device_t *bdev, uint64_t lba, uint32_t count, const void *buffer);

// achi_port.c
int ahci_port_read_sectors(ahci_port_t *port, uint64_t lba, uint32_t count, void *buf);
int ahci_port_write_sectors(ahci_port_t *port, uint64_t lba, uint32_t count, const void *buf);

#endif // AHCI_H
