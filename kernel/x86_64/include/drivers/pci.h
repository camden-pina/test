#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Legacy I/O ports for PCI config space (if ACPI MCFG isn’t available)
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

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
    uint32_t bar[6];
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

#endif
