#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Legacy I/O ports for PCI config space (if ACPI MCFG isn’t available)
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* PCI configuration space command register offset */
#define PCI_COMMAND               0x04

/* PCI Command Register bit definitions */
#define PCI_COMMAND_IO            0x1    // I/O access enable
#define PCI_COMMAND_MEMORY        0x2    // Memory access enable
#define PCI_COMMAND_BUS_MASTER    0x4    // Bus mastering enable

// PCI device structure.
typedef struct {
    uint16_t bus;
    uint16_t slot;
    uint16_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  irq;
    uint8_t  header_type;
    uint64_t bar[6];  // Changed to 64-bit to support full 64-bit BAR addresses.
} pci_device_t;

// Public functions.
void pci_init(void);
void pci_scan(void);
pci_device_t* pci_find_device(uint16_t vendor, uint16_t device);
uint32_t pci_config_read(uint16_t bus, uint16_t slot, uint16_t func, uint8_t offset);
void pci_config_write(uint16_t bus, uint16_t slot, uint16_t func, uint8_t offset, uint32_t data);
// Returns the number of PCI devices discovered.
int pci_get_device_count(void);

// Returns a pointer to the PCI device at the given index (or NULL if out of range).
pci_device_t* pci_get_device(int index);

uint8_t pci_read8(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset);
void pci_write8(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset, uint8_t data);
uint16_t pci_read16(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset);
void pci_write16(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset, uint16_t data);
uint32_t pci_read32(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset);
void pci_write32(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset, uint32_t data);

uintptr_t pci_resource_start(pci_device_t *dev, int bar_index);
uint64_t pci_resource_len(pci_device_t *dev, int bar_index);

int pci_find_capability(uint16_t bus, uint16_t slot, uint16_t func, uint8_t cap_id);

#endif
