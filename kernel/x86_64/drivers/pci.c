#include <drivers/pci.h>
#include <io.h>
#include <printf.h>
#include <acpi/acpi.h>
#include <stdint.h>

#define PCI_MAX_SLOT       32
#define PCI_MAX_FUNC       8
#define PCI_MAX_DEVICES    256

// Definitions for identifying PCI-to-PCI bridges.
#define PCI_CLASS_BRIDGE          0x06
#define PCI_SUBCLASS_PCI_BRIDGE   0x04

// Global array to hold discovered PCI devices.
static pci_device_t devices[PCI_MAX_DEVICES];
static int device_count = 0;
static uint64_t mcfg_base = 0;

/*
 * PCI IDs structure for mapping numeric IDs to friendly names.
 */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    const char *vendor_name;
    const char *device_name;
} pci_id_t;

// Minimal lookup table. Extend this table as needed.
static pci_id_t pci_ids[] = {
    { 0x8086, 0x100e, "Intel Corporation", "82540EM Gigabit Ethernet Controller" },
    { 0x10de, 0x1cb3, "NVIDIA Corporation", "GeForce GTX 1060" },
    // Add additional entries here...
    { 0x0000, 0x0000, "Unknown", "Unknown" } // Sentinel entry.
};

static const char* get_vendor_name(uint16_t vendor) {
    for (int i = 0; pci_ids[i].vendor_id != 0x0000; i++) {
        if (pci_ids[i].vendor_id == vendor)
            return pci_ids[i].vendor_name;
    }
    return "Unknown Vendor";
}

static const char* get_device_name(uint16_t vendor, uint16_t device) {
    for (int i = 0; pci_ids[i].vendor_id != 0x0000; i++) {
        if (pci_ids[i].vendor_id == vendor && pci_ids[i].device_id == device)
            return pci_ids[i].device_name;
    }
    return "Unknown Device";
}

/*
 * Initialize PCI configuration space using ACPI's MCFG table.
 * This function retrieves the base address for memory-mapped PCI configuration space.
 */
static void init_pci_config_space(void) {
    mcfg_base = acpi_get_mcfg_base();
    if (mcfg_base) {
        kprintf("MCFG base found: 0x%llx\n", mcfg_base);
        // In a complete implementation, you would parse the MCFG table here
        // to determine the valid bus range.
    } else {
        kprintf("No MCFG base found, using legacy I/O ports.\n");
    }
}

/*
 * PCI configuration read: reads 32 bits from the configuration space.
 */
uint32_t pci_config_read(uint16_t bus, uint16_t slot, uint16_t func, uint8_t offset) {
    if (mcfg_base) {
        uint64_t address = mcfg_base 
            + ((uint64_t)bus   << 20)  // 1MB per bus.
            + ((uint64_t)slot  << 15)  // 32KB per slot.
            + ((uint64_t)func  << 12)  // 4KB per function.
            + offset;
        return *(volatile uint32_t*)address;
    } else {
        uint32_t address = (0x80000000)
            | ((uint32_t)bus   << 16)
            | ((uint32_t)slot  << 11)
            | ((uint32_t)func  << 8)
            | (offset & 0xFC);
        outdw(PCI_CONFIG_ADDRESS, address);
        return indw(PCI_CONFIG_DATA);
    }
}

/*
 * PCI configuration write: writes 32 bits into the configuration space.
 */
void pci_config_write(uint16_t bus, uint16_t slot, uint16_t func, uint8_t offset, uint32_t data) {
    if (mcfg_base) {
        uint64_t address = mcfg_base 
            + ((uint64_t)bus   << 20)
            + ((uint64_t)slot  << 15)
            + ((uint64_t)func  << 12)
            + offset;
        *(volatile uint32_t*)address = data;
    } else {
        uint32_t address = (0x80000000)
            | ((uint32_t)bus   << 16)
            | ((uint32_t)slot  << 11)
            | ((uint32_t)func  << 8)
            | (offset & 0xFC);
        outdw(PCI_CONFIG_ADDRESS, address);
        outdw(PCI_CONFIG_DATA, data);
    }
}

/*
 * Helper: Check if the vendor value is valid.
 * A vendor value of 0xFFFF indicates no device.
 */
static int is_valid_device(uint16_t vendor) {
    return (vendor != 0xFFFF);
}

/*
 * Helper: Determine if a device is a PCI-to-PCI bridge.
 */
static int is_pci_bridge(uint16_t bus, uint16_t slot, uint16_t func) {
    uint32_t class_reg = pci_config_read(bus, slot, func, 8);
    uint8_t class_code = (class_reg >> 24) & 0xFF;
    uint8_t subclass   = (class_reg >> 16) & 0xFF;
    return (class_code == PCI_CLASS_BRIDGE && subclass == PCI_SUBCLASS_PCI_BRIDGE);
}

/*
 * Probe a single function on a given bus and slot.
 * Validates the data read from PCI config space before populating the devices array.
 */
static void pci_probe_function(uint16_t bus, uint16_t slot, uint16_t func) {
    uint32_t data = pci_config_read(bus, slot, func, 0);
    uint16_t vendor = data & 0xFFFF;
    if (!is_valid_device(vendor))
        return; // No valid device here.
    
    if (device_count >= PCI_MAX_DEVICES) {
        kprintf("PCI device array full, skipping device at Bus %d, Slot %d, Func %d\n", bus, slot, func);
        return;
    }
    
    pci_device_t *dev = &devices[device_count++];
    dev->bus = bus;
    dev->slot = slot;
    dev->function = func;
    dev->vendor_id = vendor;
    dev->device_id = (data >> 16) & 0xFFFF;
    
    uint32_t class_reg = pci_config_read(bus, slot, func, 8);
    dev->class_code = (class_reg >> 24) & 0xFF;
    dev->subclass = (class_reg >> 16) & 0xFF;
    dev->prog_if = (class_reg >> 8) & 0xFF;
    dev->header_type = pci_config_read(bus, slot, func, 0x0C) & 0xFF;
    
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_config_read(bus, slot, func, 0x10 + i * 4);
    }
    
    kprintf("PCI Device: Bus %d, Slot %d, Func %d, Vendor: %s (0x%x), Device: %s (0x%x), Class: 0x%x\n",
           bus, slot, func, get_vendor_name(dev->vendor_id), dev->vendor_id,
           get_device_name(dev->vendor_id, dev->device_id), dev->device_id,
           dev->class_code);
}

/*
 * Recursively scan a given bus for devices.
 * Uses the standard method of scanning bus 0 and then following PCI-to-PCI bridges.
 */
static void pci_scan_bus(uint16_t bus) {
    for (uint16_t slot = 0; slot < PCI_MAX_SLOT; slot++) {
        for (uint16_t func = 0; func < PCI_MAX_FUNC; func++) {
            uint32_t data = pci_config_read(bus, slot, func, 0);
            uint16_t vendor = data & 0xFFFF;
            if (!is_valid_device(vendor)) {
                // If function 0 is invalid, no need to check further functions for this slot.
                if (func == 0)
                    break;
                else
                    continue;
            }
            
            pci_probe_function(bus, slot, func);
            
            // If the device is a PCI-to-PCI bridge, scan its secondary bus.
            if (is_pci_bridge(bus, slot, func)) {
                uint32_t bus_numbers = pci_config_read(bus, slot, func, 0x18);
                uint8_t secondary_bus = (bus_numbers >> 8) & 0xFF;
                // Prevent infinite recursion: only scan if the secondary bus is greater than the current.
                if (secondary_bus > bus) {
                    kprintf("PCI Bridge at Bus %d, Slot %d, Func %d: Scanning secondary bus %d\n",
                            bus, slot, func, secondary_bus);
                    pci_scan_bus(secondary_bus);
                }
            }
            
            // For non-multi-function devices, no need to scan additional functions.
            if (func == 0) {
                uint8_t header_type = pci_config_read(bus, slot, 0, 0x0C) & 0xFF;
                if (!(header_type & 0x80))
                    break;
            }
        }
    }
}

/*
 * Print a summary of all discovered PCI devices.
 */
void pci_print_devices(void) {
    kprintf("----- PCI Scan Summary -----\n");
    kprintf("Total PCI devices found: %d\n", device_count);
    for (int i = 0; i < device_count; i++) {
        pci_device_t *dev = &devices[i];
        kprintf("Device %d: Bus %d, Slot %d, Func %d, Vendor: %s (0x%x), Device: %s (0x%x), Class: 0x%x\n",
                i, dev->bus, dev->slot, dev->function,
                get_vendor_name(dev->vendor_id), dev->vendor_id,
                get_device_name(dev->vendor_id, dev->device_id), dev->device_id,
                dev->class_code);
    }
    kprintf("-----------------------------\n");
}

/*
 * Public function to initiate a PCI scan.
 */
void pci_scan(void) {
    device_count = 0;
    pci_scan_bus(0);
}

/*
 * Public initialization function for the PCI subsystem.
 * This function initializes the configuration space, scans the PCI buses,
 * and prints a summary of the discovered devices.
 */
void pci_init(void) {
    kprintf("Initializing PCI subsystem...\n");
    init_pci_config_space();
    pci_scan();
    pci_print_devices();
}

/*
 * Lookup function: Find a PCI device by vendor and device ID.
 */
pci_device_t* pci_find_device(uint16_t vendor, uint16_t device) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].vendor_id == vendor && devices[i].device_id == device)
            return &devices[i];
    }
    return 0;
}
