#include <msi.h>
#include <drivers/pci.h>
#include <interrupts/lapic.h>    // For reading APIC base via MSR if needed.
#include <msr.h>
#include <printf.h>
#include <io.h>
#include <mm/pgtable.h>
#include <mm/pmm.h>
#include <mm/vmem.h>
#include <string.h>

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
    kprintf("phys_addr: %llx\n", phys_addr);
    void *mapped = (void *)vmap_phys(phys_addr, 0, pages * PAGE_SIZE, vm_flags, "msi table");

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
 * supports at least the requested number of vectors, maps the MSI-X table and the MSI-X
 * Pending Bit Array (PBA) from the BARs, initializes each table entry with the message
 * address/data, clears the PBA, and then updates the MSI-X capability structure:
 *  - The Table Offset field is set to the MSI-X Message Table.
 *  - The PBA Offset field is set to the MSI-X Pending Bit Array.
 * Finally, it clears the function mask and enables MSI-X.
 */
int pci_enable_msix(pci_device_t *dev, uint8_t vector_base, uint16_t num_vectors) {
    int cap_offset = pci_find_capability(dev->bus, dev->slot, dev->function, 0x11);
    if (!cap_offset) {
        kprintf("MSI-X capability not found for device %02x:%02x.%x\n",
                dev->bus, dev->slot, dev->function);
        return -1;
    }
    kprintf("MSI-X capability found at offset 0x%x\n", cap_offset);

    // Read the MSI-X Message Control register and determine table size.
    uint16_t msg_control = pci_read16(dev->bus, dev->slot, dev->function, cap_offset + 2);
    kprintf("Initial MSI-X Message Control: 0x%x\n", msg_control);
    uint16_t table_size = (msg_control & 0x07FF) + 1;
    kprintf("MSI-X table size (number of vectors): %u\n", table_size);
    
    if (num_vectors > table_size) {
        kprintf("Requested num_vectors %u exceeds device MSI-X table size %u\n",
                num_vectors, table_size);
        return -1;
    }

    // --- Map the MSI-X Message Table ---
    // Read the Table information from the MSI-X capability structure (offset 4)
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
    size_t table_mapping_size = num_vectors * sizeof(msix_table_entry_t);
    kprintf("Mapping MSI-X table region: size=0x%lx bytes\n", table_mapping_size);

    // --- Ensure table physical address is page aligned ---
    size_t table_pgoff = msix_table_phys % PAGE_SIZE;
    uintptr_t aligned_msix_table_phys = msix_table_phys - table_pgoff;
    size_t aligned_table_mapping_size = table_mapping_size + table_pgoff;

    kprintf("aligned_msix_table_phys: %lx, aligned_table_mappins_size: %lx\n", aligned_msix_table_phys, aligned_table_mapping_size);
    vm_print_address_space();
    print_buddy_debug();
    msix_table_entry_t *mapped_table_region = (msix_table_entry_t*)ioremap(aligned_msix_table_phys, aligned_table_mapping_size, "msix table");
    /*msix_table_entry_t *mapped_table_region = (msix_table_entry_t*)vmap_phys(
            aligned_msix_table_phys,
            0,
            ((aligned_table_mapping_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE,
            VM_WRITE | VM_NOCACHE,
            "msi table");*/
        // map_physical_region(aligned_msix_table_phys, aligned_table_mapping_size, VM_WRITE | VM_NOCACHE);
    if (!mapped_table_region) {
        kprintf("Failed to map MSI-X table region\n");
        return -1;
    }
    msix_table_entry_t *msix_table = (msix_table_entry_t *)((uintptr_t)mapped_table_region + table_pgoff);
    kprintf("MSI-X table mapped at virtual address 0x%p\n", msix_table);

    // --- Map the MSI-X Pending Bit Array (PBA) ---
    // Read the PBA information from the MSI-X capability structure (offset 8)
    uint32_t pba_info = pci_config_read(dev->bus, dev->slot, dev->function, cap_offset + 8);
    uint8_t pba_bar = pba_info & 0x7;
    uint32_t pba_offset = pba_info & ~0x7;
    if (pba_bar >= 6) {
        kprintf("Invalid BAR index %u for MSI-X PBA\n", pba_bar);
        return -1;
    }
    uintptr_t pba_bar_base = dev->bar[pba_bar];
    uintptr_t msix_pba_phys = pba_bar_base + pba_offset;
    // The PBA is a bit array with one bit per MSI-X vector.
    // Calculate the number of 32-bit words required (rounding up).
    size_t pba_mapping_size = ((table_size + 31) / 32) * sizeof(uint32_t);
    kprintf("Mapping MSI-X PBA region: size=0x%zx bytes\n", pba_mapping_size);

    // --- Ensure PBA physical address is page aligned ---
    size_t pba_pgoff = msix_pba_phys % PAGE_SIZE;
    uintptr_t aligned_msix_pba_phys = msix_pba_phys - pba_pgoff;
    size_t aligned_pba_mapping_size = pba_mapping_size + pba_pgoff;

    kprintf("aligned_msix_table_phys: %lx, aligned_table_mappins_size: %lx\n", aligned_msix_pba_phys, aligned_pba_mapping_size);
    uint32_t *mapped_pba_region = (uint32_t*)ioremap(aligned_msix_pba_phys, aligned_pba_mapping_size, "msix pba");
    /*uint32_t *mapped_pba_region = (uint32_t*)vmap_phys(
            aligned_msix_pba_phys,
            0,
            ((aligned_pba_mapping_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE,
            VM_WRITE | VM_NOCACHE,
            "msi pba");*/
        // map_physical_region(aligned_msix_pba_phys, aligned_pba_mapping_size, VM_WRITE | VM_NOCACHE | VM_FIXED);
    if (!mapped_pba_region) {
        kprintf("Failed to map MSI-X PBA region\n");
        return -1;
    }
    uint32_t *msix_pba = (uint32_t *)((uintptr_t)mapped_pba_region + pba_pgoff);
    kprintf("MSI-X PBA mapped at virtual address 0x%p\n", msix_pba);
    
    // Initialize the PBA by clearing all bits.
    memset(msix_pba, 0, pba_mapping_size);
    kprintf("MSI-X PBA initialized (all bits cleared)\n");

    // --- Initialize MSI-X Table Entries ---
    uint64_t apic_base = read_msr(MSR_IA32_APIC_BASE) & 0xFFFFF000ULL;
    uint32_t msg_addr = (uint32_t) apic_base;
    kprintf("LAPIC base (for MSI-X message address): 0x%x\n", msg_addr);
    
    for (uint16_t i = 0; i < num_vectors; i++) {
        msix_table[i].msg_addr_lo = msg_addr;
        msix_table[i].msg_addr_hi = 0;
        msix_table[i].msg_data    = vector_base + i;
        msix_table[i].vector_control = 0;  // Unmask the entry.
        kprintf("MSI-X table entry %u: msg_addr_lo=0x%x, msg_addr_hi=0x%x, msg_data=0x%x, vector_control=0x%x\n",
                i, msix_table[i].msg_addr_lo, msix_table[i].msg_addr_hi,
                msix_table[i].msg_data, msix_table[i].vector_control);
    }
    
    // --- Update the MSI-X Capability Structure ---
    // Reprogram the Table and PBA Offset fields so that they point to the allocated regions.
    // (If the physical layout was relocated, update the offset portions accordingly.)
    pci_write32(dev->bus, dev->slot, dev->function, cap_offset + 4, table_info);
    pci_write32(dev->bus, dev->slot, dev->function, cap_offset + 8, pba_info);
    kprintf("MSI-X capability structure updated with table and PBA offsets\n");

    // --- Update the MSI-X Message Control register ---
    // Clear the Function Mask (bit 14) and set the MSI-X Enable bit (bit 15)
    msg_control = (msg_control & ~(1 << 14)) | (1 << 15);
    kprintf("Writing updated MSI-X Message Control: 0x%x\n", msg_control);
    pci_write16(dev->bus, dev->slot, dev->function, cap_offset + 2, msg_control);

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

