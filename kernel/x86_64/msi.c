#include <msi.h>
#include <drivers/pci.h>
#include <interrupts/lapic.h>    // For reading APIC base via MSR if needed.
#include <msr.h>
#include <printf.h>
#include <io.h>
#include <mm/pgtable.h>
#include <mm/pmm.h>

extern uintptr_t next_free_virt;  // declared in your pgtable.c

#define PAGE_SIZE 0x1000

/**
 * map_physical_region - Map an existing physical region into virtual memory.
 *
 * @phys_addr: The physical start address of the region.
 * @size:      The size (in bytes) of the region to map.
 * @vm_flags:  The VM flags to use (e.g., VM_WRITE | VM_NOCACHE).
 *
 * Returns: A pointer to the virtual address mapping, or NULL on failure.
 */
void *map_physical_region(uintptr_t phys_addr, size_t size, uint32_t vm_flags) {
    // Round up size to page boundary.
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t vaddr = next_free_virt;
    void *mapped = early_map_entries(vaddr, phys_addr, pages, vm_flags);
    if (mapped) {
        next_free_virt += pages * PAGE_SIZE;
    }
    return mapped;
}

/*
 * Enables MSI for the given PCI device.
 *
 * This function locates the MSI capability (cap ID 0x05) in the PCI config space,
 * programs the message address (using the LAPIC base from the MSR) and message data,
 * and sets the MSI enable bit.
 */
int pci_enable_msi(pci_device_t *dev, uint8_t vector) {
    int cap_offset = pci_find_capability(dev->bus, dev->slot, dev->function, 0x05);
    if (!cap_offset) {
        kprintf("MSI capability not found for device %02x:%02x.%x\n", dev->bus, dev->slot, dev->function);
        return -1;
    }
    kprintf("MSI capability found at offset 0x%x\n", cap_offset);

    // Read the MSI Message Control register (16-bit) at offset cap_offset+2.
    uint16_t msg_control = pci_read16(dev->bus, dev->slot, dev->function, cap_offset + 2);
    // Bit 7 of the message control indicates 64-bit addressing.
    int is_64bit = msg_control & (1 << 7);
    kprintf("MSI: 64-bit capable: %s\n", is_64bit ? "Yes" : "No");

    // Obtain the LAPIC physical base address (typically 0xFEE00000).
    // Here we read from the APIC Base MSR.
    uint64_t apic_base = read_msr(MSR_IA32_APIC_BASE) & 0xFFFFF000ULL;
    uint32_t msg_addr = (uint32_t)apic_base; // Standard message address for local APIC.
    uint16_t msg_data = vector;             // Use the provided vector (you can add delivery mode bits if needed).

    if (is_64bit) {
        // For 64-bit MSI capability:
        //   - Offset cap_offset+4: Lower 32 bits of the Message Address.
        //   - Offset cap_offset+8: Upper 32 bits (usually 0).
        //   - Offset cap_offset+12: Message Data.
        pci_config_write(dev->bus, dev->slot, dev->function, cap_offset + 4, msg_addr);
        pci_config_write(dev->bus, dev->slot, dev->function, cap_offset + 8, 0);
        pci_config_write(dev->bus, dev->slot, dev->function, cap_offset + 12, msg_data);
    } else {
        // For 32-bit MSI capability:
        //   - Offset cap_offset+4: Message Address.
        //   - Offset cap_offset+8: Message Data.
        pci_config_write(dev->bus, dev->slot, dev->function, cap_offset + 4, msg_addr);
        pci_config_write(dev->bus, dev->slot, dev->function, cap_offset + 8, msg_data);
    }
    // Finally, set the MSI Enable bit (bit 0) in the Message Control register.
    msg_control |= 0x0001;
    pci_write16(dev->bus, dev->slot, dev->function, cap_offset + 2, msg_control);

    kprintf("MSI enabled for device %02x:%02x.%x with vector 0x%x\n",
            dev->bus, dev->slot, dev->function, vector);
    return 0;
}

/*
 * MSI-X table entry structure.
 * Each entry consists of:
 *   - Message Address (64-bit: low and high 32 bits)
 *   - Message Data (32-bit)
 *   - Vector Control (32-bit): Bit 0 is the mask bit.
 */
typedef struct __attribute__((packed)) msix_table_entry {
    uint32_t msg_addr_lo;
    uint32_t msg_addr_hi;
    uint32_t msg_data;
    uint32_t vector_control; // 0 = unmasked; bit0 set masks the entry.
} msix_table_entry_t;

/*
 * Enables MSI-X for the given PCI device.
 *
 * This function locates the MSI-X capability (cap ID 0x11), verifies that the device
 * supports at least the requested number of vectors, maps the MSI-X table from the BAR,
 * initializes each table entry with the message address/data, and then clears the function mask.
 */
int pci_enable_msix(pci_device_t *dev, uint8_t vector_base, uint16_t num_vectors) {
    int cap_offset = pci_find_capability(dev->bus, dev->slot, dev->function, 0x11);
    if (!cap_offset) {
        kprintf("MSI-X capability not found for device %02x:%02x.%x\n",
                dev->bus, dev->slot, dev->function);
        return -1;
    }
    kprintf("MSI-X capability found at offset 0x%x\n", cap_offset);

    uint16_t msg_control = pci_read16(dev->bus, dev->slot, dev->function, cap_offset + 2);
    uint16_t table_size = (msg_control & 0x07FF) + 1;
    if (num_vectors > table_size) {
        kprintf("Requested num_vectors %u exceeds device MSI-X table size %u\n",
                num_vectors, table_size);
        return -1;
    }

    uint32_t table_info = pci_config_read(dev->bus, dev->slot, dev->function, cap_offset + 4);
    uint8_t table_bar = table_info & 0x7;
    uint32_t table_offset = table_info & ~0x7;
    if (table_bar >= 6) {
        kprintf("Invalid BAR index %u for MSI-X table\n", table_bar);
        return -1;
    }
    uintptr_t bar_base = dev->bar[table_bar];
    uintptr_t msix_table_phys = bar_base + table_offset;
    kprintf("MSI-X table: BAR %u, physical address=0x%lx\n", table_bar, msix_table_phys);

    // Calculate the size needed for the MSI-X table.
    size_t mapping_size = (num_vectors * sizeof(msix_table_entry_t));
    // Use our helper to map the physical region into virtual address space.
    msix_table_entry_t *msix_table = (msix_table_entry_t *)
                                     map_physical_region(msix_table_phys, mapping_size, VM_WRITE | VM_NOCACHE);
    if (!msix_table) {
        kprintf("Failed to map MSI-X table region\n");
        return -1;
    }

    uint64_t apic_base = read_msr(MSR_IA32_APIC_BASE) & 0xFFFFF000ULL;
    uint32_t msg_addr = (uint32_t)apic_base;
    
    for (uint16_t i = 0; i < num_vectors; i++) {
        msix_table[i].msg_addr_lo = msg_addr;
        msix_table[i].msg_addr_hi = 0;
        msix_table[i].msg_data    = vector_base + i;
        msix_table[i].vector_control = 0;  // Unmask the entry.
    }
    
    msg_control = (msg_control & ~(1 << 14)) | (1 << 15);
    pci_write16(dev->bus, dev->slot, dev->function, cap_offset + 2, msg_control);

//    msg_control &= ~(1 << 14);  // Clear the MSI-X Function Mask.
 //   pci_write16(dev->bus, dev->slot, dev->function, cap_offset + 2, msg_control);

    kprintf("MSI-X enabled for device %02x:%02x.%x with base vector 0x%x (%u vectors)\n",
            dev->bus, dev->slot, dev->function, vector_base, num_vectors);
    return 0;
}

/*
void init_device_interrupts(void) {
    // Assume pci_init() has already been called.
    pci_device_t *dev = pci_find_device(0x8086, 0x100E);  // Replace with your device’s vendor and device IDs.
    if (dev) {
        if (pci_enable_msi(dev, 0x40) != 0) {
            kprintf("Failed to enable MSI on device %02x:%02x.%x\n", dev->bus, dev->slot, dev->function);
        }
        // Alternatively, for MSI-X:
        if (pci_enable_msix(dev, 0x50, 4) != 0) {
            kprintf("Failed to enable MSI-X on device %02x:%02x.%x\n", dev->bus, dev->slot, dev->function);
        }
    } else {
        kprintf("Device not found!\n");
    }
}
*/

void init_device_interrupts(void) {
    int count = pci_get_device_count();
    kprintf("Found %d PCI devices\n", count);
    
    for (int i = 0; i < count; i++) {
        pci_device_t *dev = pci_get_device(i);
        kprintf("Device %02x:%02x.%x - Vendor: 0x%04x, Device: 0x%04x\n",
                dev->bus, dev->slot, dev->function,
                dev->vendor_id, dev->device_id);
        
        // Check if MSI is supported
        int msi_cap = pci_find_capability(dev->bus, dev->slot, dev->function, 0x05);
        if (msi_cap) {
            kprintf("  MSI capability found at offset 0x%x, enabling MSI...\n", msi_cap);
            if (pci_enable_msi(dev, 0x40) != 0) {
                kprintf("  Failed to enable MSI on device %02x:%02x.%x\n",
                        dev->bus, dev->slot, dev->function);
            } else {
                kprintf("  MSI enabled for device %02x:%02x.%x\n",
                        dev->bus, dev->slot, dev->function);
            }
        } else {
            // Check if MSI-X is supported
            int msix_cap = pci_find_capability(dev->bus, dev->slot, dev->function, 0x11);
            if (msix_cap) {
                kprintf("  MSI-X capability found at offset 0x%x, enabling MSI-X...\n", msix_cap);
                if (pci_enable_msix(dev, 0x50, 4) != 0) {
                    kprintf("  Failed to enable MSI-X on device %02x:%02x.%x\n",
                            dev->bus, dev->slot, dev->function);
                } else {
                    kprintf("  MSI-X enabled for device %02x:%02x.%x\n",
                            dev->bus, dev->slot, dev->function);
                }
            } else {
                kprintf("  Device %02x:%02x.%x does not support MSI/MSI-X. Using legacy interrupts.\n",
                        dev->bus, dev->slot, dev->function);
            }
        }
    }
}

