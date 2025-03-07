#ifndef MSI_H
#define MSI_H

#include <stdint.h>
#include <drivers/pci.h>

/**
 * Enables MSI on the given PCI device.
 *
 * @param dev    Pointer to the PCI device structure.
 * @param vector The interrupt vector to be used.
 * @return 0 on success, nonzero on error.
 */
int pci_enable_msi(pci_device_t *dev, uint8_t vector);

/**
 * Enables MSI-X on the given PCI device.
 *
 * @param dev         Pointer to the PCI device structure.
 * @param vector_base The base interrupt vector to be used for the table.
 * @param num_vectors The number of vectors to allocate (must not exceed the device’s capability).
 * @return 0 on success, nonzero on error.
 */
int pci_enable_msix(pci_device_t *dev, uint8_t vector_base, uint16_t num_vectors);

void init_device_interrupts(void);

#endif
