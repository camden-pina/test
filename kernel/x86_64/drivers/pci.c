#include <drivers/pci.h>
#include <io.h>
#include <printf.h>
#include <acpi/acpi.h>
#include <panic.h>

#define PCI_MAX_FUNCTIONS 8
#define PCI_MAX_DEVICES   256

// Global array of discovered PCI devices
static pci_device_t pci_devices[PCI_MAX_DEVICES];
static int pci_device_count = 0;
static uint64_t pci_mcfg_base = 0;

// Lookup table for common PCI device names (for logging)
static const struct {
    uint16_t vendor, device;
    const char *vendor_name, *device_name;
} pci_known_ids[] = {

    // -------------------- INTEL --------------------
    // Intel Ethernet Controllers
    {0x8086, 0x100E, "Intel", "82540EM Gigabit Ethernet"},
    {0x8086, 0x1533, "Intel", "I210 Gigabit Network Connection"},
    {0x8086, 0x10D3, "Intel", "82574L Gigabit Network Connection"},
    {0x8086, 0x15B8, "Intel", "Ethernet Connection I219-V"},

    // Intel DRAM Controllers
    {0x8086, 0x29c0, "Intel", "82G33/G31/P35/P31 Express DRAM Controller"},
    
    // Intel Graphics (iGPU)
    {0x8086, 0x3E98, "Intel", "UHD Graphics 630"},
    {0x8086, 0x5912, "Intel", "HD Graphics 630"},
    {0x8086, 0x9BC5, "Intel", "Iris Xe Graphics"},
    
    // Intel USB Controllers
    {0x8086, 0x8C31, "Intel", "USB 3.0 eXtensible Host Controller"},
    {0x8086, 0xA36D, "Intel", "Cannon Lake PCH USB 3.1 xHCI Controller"},
    {0x8086, 0x24cd, "Intel", "82801DB/DBM (ICH4/ICH4-M) USB2 EHCI Controller"},
    {0x8086, 0x2918, "Intel", "82801IB (ICH9) LPC Interface Controller"},
    {0x8086, 0x2930, "Intel", "82801I (ICH9 Family) SMBus Controller"},
    
    // Intel SATA Controllers
    {0x8086, 0x2922, "Intel", "ICH9 AHCI SATA Controller"},
    {0x8086, 0x8C02, "Intel", "8 Series/C220 Series AHCI Controller"},
    
    // Intel NVMe Controllers
    {0x8086, 0x0953, "Intel", "SSD 750 Series NVMe Controller"},

    // -------------------- AMD --------------------
    // AMD Graphics (Radeon)
    {0x1002, 0x67DF, "AMD", "Radeon RX 580"},
    {0x1002, 0x731F, "AMD", "Radeon RX 5700 XT"},
    {0x1002, 0x73BF, "AMD", "Radeon RX 6800 XT"},
    
    // AMD Ethernet
    {0x1022, 0x2000, "AMD", "PCnet-PCI Ethernet Adapter"},

    // AMD USB Controllers
    {0x1022, 0x145C, "AMD", "X370 USB 3.1 Controller"},
    
    // AMD SATA Controllers
    {0x1022, 0x7901, "AMD", "SB7x0/SB8x0/SB9x0 SATA Controller"},

    // -------------------- NVIDIA --------------------
    {0x10DE, 0x1C82, "NVIDIA", "GeForce GTX 1050 Ti"},
    {0x10DE, 0x2184, "NVIDIA", "GeForce RTX 3060"},
    {0x10DE, 0x2204, "NVIDIA", "GeForce RTX 3080 Ti"},
    {0x10DE, 0x2482, "NVIDIA", "GeForce RTX 4090"},

    // -------------------- REALTEK --------------------
    {0x10EC, 0x8139, "Realtek", "RTL-8139 Fast Ethernet"},
    {0x10EC, 0x8168, "Realtek", "RTL8111/8168/8411 PCI Express Gigabit Ethernet"},
    {0x10EC, 0x8125, "Realtek", "RTL8125 2.5Gb Ethernet"},

    // -------------------- BROADCOM --------------------
    {0x14E4, 0x165F, "Broadcom", "NetXtreme BCM5719 Gigabit Ethernet"},
    {0x14E4, 0x16A1, "Broadcom", "BCM57416 NetXtreme-E 10Gb Ethernet"},
    
    // Broadcom Wi-Fi
    {0x14E4, 0x43A0, "Broadcom", "BCM4360 802.11ac Wireless Network Adapter"},
    {0x14E4, 0x43B1, "Broadcom", "BCM4352 802.11ac Wireless Network Adapter"},

    // -------------------- QUALCOMM ATHEROS --------------------
    {0x168C, 0x003E, "Qualcomm Atheros", "QCA6174 802.11ac Wireless Network Adapter"},
    {0x168C, 0x0042, "Qualcomm Atheros", "QCA9377 802.11ac Wireless Network Adapter"},

    // -------------------- ASMEDIA --------------------
    {0x1B21, 0x0612, "ASMedia", "ASM1062 SATA Controller"},
    {0x1B21, 0x1242, "ASMedia", "ASM2142 USB 3.1 Host Controller"},

    // -------------------- MARVELL --------------------
    {0x1B4B, 0x9235, "Marvell", "88SE9235 PCIe SATA Controller"},

    // -------------------- VMWARE & VIRTUALIZATION --------------------
    {0x15AD, 0x0405, "VMware", "VMXNET3 Ethernet Adapter"},
    {0x15AD, 0x0770, "VMware", "SVGA II Adapter"},
    {0x1AF4, 0x1000, "VirtIO", "VirtIO Network Adapter"},
    {0x1AF4, 0x1042, "VirtIO", "VirtIO Block Device"},
    {0x1AF4, 0x1052, "VirtIO", "VirtIO GPU Device"},

    // -------------------- USB CONTROLLERS --------------------
    {0x1033, 0x0194, "NEC", "USB 3.0 Host Controller"},
    {0x1B6F, 0x7052, "Etron", "USB 3.0 xHCI Controller"},
    {0x1D6A, 0x07B1, "Google", "Titan Security Key"},

    // -------------------- SOUND CARDS --------------------
    {0x8086, 0xA170, "Intel", "HD Audio Controller"},
    {0x1102, 0x0004, "Creative", "Sound Blaster Audigy"},
    {0x10EC, 0x0887, "Realtek", "ALC887 HD Audio Codec"},
    
    // -------------------- STORAGE --------------------
    {0x144D, 0xA808, "Samsung", "NVMe SSD Controller (Samsung 970 EVO)"},
    {0x1C5C, 0x1327, "Phison", "E12 NVMe SSD Controller"},

    // -------------------- FALLBACK --------------------
    {0, 0, "Unknown", "Unknown"}
};

static const char* pci_lookup_vendor(uint16_t vid) {
    for (int i = 0; pci_known_ids[i].vendor; ++i)
        if (pci_known_ids[i].vendor == vid) return pci_known_ids[i].vendor_name;
    return "Unknown Vendor";
}
static const char* pci_lookup_device(uint16_t vid, uint16_t did) {
    for (int i = 0; pci_known_ids[i].vendor; ++i) {
        if (pci_known_ids[i].vendor == vid && pci_known_ids[i].device == did)
            return pci_known_ids[i].device_name;
    }
    return "Unknown Device";
}

// Initialize PCI config access (use ACPI MCFG if present)
static void pci_config_space_init(void) {
    pci_mcfg_base = acpi_get_mcfg_base();
//    pci_mcfg_base = pci_mcfg_base + 0x2C;
    pci_mcfg_base = *(uint64_t *)(pci_mcfg_base + 0x2C); // Read the actual PCIe base address

    if (pci_mcfg_base) {
        kprintf("PCI: Using PCIe MMCONFIG at 0x%llx\n", pci_mcfg_base);
    } else {
        kprintf("PCI: Using legacy I/O config (0xCF8/0xCFC)\n");
    }
}

// Read a 32-bit value from PCI config space
uint32_t pci_config_read(uint16_t bus, uint16_t slot, uint16_t func, uint8_t offset) {
    if (pci_mcfg_base) {
        uint64_t addr = pci_mcfg_base 
                      + ((uint64_t)bus << 20) 
                      + ((uint64_t)slot << 15) 
                      + ((uint64_t)func << 12) 
                      + (offset & 0xFFF);
        return *(volatile uint32_t*)addr;
    } else {
        uint32_t address = 0x80000000 
                         | ((uint32_t)bus << 16) 
                         | ((uint32_t)slot << 11) 
                         | ((uint32_t)func << 8) 
                         | (offset & 0xFC);
        outdw(PCI_CONFIG_ADDRESS, address);
        return indw(PCI_CONFIG_DATA);
    }
}

// Write a 32-bit value to PCI config space
void pci_config_write(uint16_t bus, uint16_t slot, uint16_t func, uint8_t offset, uint32_t value) {
    if (pci_mcfg_base) {
        uint64_t addr = pci_mcfg_base 
                      + ((uint64_t)bus << 20) 
                      + ((uint64_t)slot << 15) 
                      + ((uint64_t)func << 12) 
                      + (offset & 0xFFF);
        *(volatile uint32_t*)addr = value;
    } else {
        uint32_t address = 0x80000000 
                         | ((uint32_t)bus << 16) 
                         | ((uint32_t)slot << 11) 
                         | ((uint32_t)func << 8) 
                         | (offset & 0xFC);
        outdw(PCI_CONFIG_ADDRESS, address);
        outdw(PCI_CONFIG_DATA, value);
    }
}

/**
 * Read a 16-bit value from PCI configuration space.
 *
 * This simply calls the existing 32-bit read and extracts the correct half-word.
 */
uint16_t pci_read16(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset) {
    // Force alignment to a 4-byte boundary (the offset must start at an even multiple of 4)
    uint8_t aligned_offset = offset & 0xFC;

    // Read the entire 32-bit value
    uint32_t value = pci_config_read(bus, slot, function, aligned_offset);

    // Depending on the offset’s lower 2 bits, we shift appropriately
    // If offset ends with 0b00, we want bits [15..0]. If it ends with 0b10, we want bits [31..16].
    int shift = (offset & 2) * 8;

    return (uint16_t)((value >> shift) & 0xFFFF);
}

/**
 * Write a 16-bit value to PCI configuration space.
 *
 * This reads the existing 32-bit value, updates the desired half-word, and writes back.
 */
void pci_write16(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset, uint16_t data) {
    uint8_t aligned_offset = offset & 0xFC;
    uint32_t oldval = pci_config_read(bus, slot, function, aligned_offset);

    // Shift determines which half of the 32 bits we’re changing
    int shift = (offset & 2) * 8;

    // Mask out the old 16 bits
    uint32_t mask = 0xFFFF << shift;
    uint32_t newval = (oldval & ~mask) | ((data & 0xFFFF) << shift);

    pci_config_write(bus, slot, function, aligned_offset, newval);
}

/**
 * Read a 32-bit value from PCI configuration space.
 *
 * This is just a direct pass-through to pci_config_read.
 */
uint32_t pci_read32(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset) {
    // We trust offset is suitably aligned. If needed, we could do offset &= 0xFC here.
    return pci_config_read(bus, slot, function, offset);
}

/**
 * Write a 32-bit value to PCI configuration space.
 *
 * This is just a direct pass-through to pci_config_write.
 */
void pci_write32(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset, uint32_t data) {
    pci_config_write(bus, slot, function, offset, data);
}

// Check if a PCI device is present (vendor ID != 0xFFFF)
static inline int pci_device_exists(uint16_t vendor_id) {
    return (vendor_id != 0xFFFF);
}

// Determine if a PCI function is a bridge (to scan subordinate bus)
static int pci_is_bridge(uint16_t bus, uint16_t slot, uint16_t func) {
    uint32_t class_val = pci_config_read(bus, slot, func, 0x08);
    uint8_t base_class = (class_val >> 24) & 0xFF;
    uint8_t sub_class  = (class_val >> 16) & 0xFF;
    return (base_class == 0x06 && sub_class == 0x04);
}

// Probe a single PCI function and record it
static void pci_probe_function(uint16_t bus, uint16_t slot, uint16_t func) {
    uint32_t id_reg = pci_config_read(bus, slot, func, 0x00);
    uint16_t vendor_id = id_reg & 0xFFFF;
    if (!pci_device_exists(vendor_id)) return;
    if (pci_device_count >= PCI_MAX_DEVICES) {
        panic("PCI: device list overflow");
    }
    // Populate device structure
    pci_device_t *dev = &pci_devices[pci_device_count++];
    dev->bus = bus; dev->slot = slot; dev->function = func;
    dev->vendor_id = vendor_id;
    dev->device_id = (id_reg >> 16) & 0xFFFF;
    uint32_t class_reg = pci_config_read(bus, slot, func, 0x08);
    dev->class_code = (class_reg >> 24) & 0xFF;
    dev->subclass  = (class_reg >> 16) & 0xFF;
    dev->prog_if   = (class_reg >> 8) & 0xFF;
    uint32_t hdr_reg = pci_config_read(bus, slot, func, 0x0C);
    dev->header_type = (hdr_reg >> 16) & 0xFF;
    // Read Base Address Registers
    for (int bar = 0; bar < 6; ++bar) {
        dev->bar[bar] = pci_config_read(bus, slot, func, 0x10 + bar*4);
    }
    // Enable memory/IO decoding and bus mastering for this device
    uint32_t cmd_reg = pci_config_read(bus, slot, func, 0x04);
    cmd_reg |= 0x0007;  // enable I/O space, memory space, and bus master
    pci_config_write(bus, slot, func, 0x04, cmd_reg);
    // Log the device
    kprintf("PCI: [%02x:%02x.%x] %s (VID 0x%04x) - %s (DID 0x%04x), Class 0x%02x Subclass 0x%02x\n",
            bus, slot, func,
            pci_lookup_vendor(dev->vendor_id), dev->vendor_id,
            pci_lookup_device(dev->vendor_id, dev->device_id), dev->device_id,
            dev->class_code, dev->subclass);
}

// Recursively scan all PCI buses reachable from a given bus
static void pci_scan_bus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; ++slot) {
        uint32_t id = pci_config_read(bus, slot, 0, 0x00);
        uint16_t vendor = id & 0xFFFF;
        if (!pci_device_exists(vendor)) continue;
        // Determine function count
        uint8_t header_type = (pci_config_read(bus, slot, 0, 0x0C) >> 16) & 0xFF;
        int func_count = (header_type & 0x80) ? PCI_MAX_FUNCTIONS : 1;
        for (uint8_t func = 0; func < func_count; ++func) {
            uint32_t func_id = pci_config_read(bus, slot, func, 0x00);
            uint16_t vend = func_id & 0xFFFF;
            if (!pci_device_exists(vend)) continue;
            pci_probe_function(bus, slot, func);
            // If this function is a PCI-PCI bridge, scan its secondary bus
            if (pci_is_bridge(bus, slot, func)) {
                uint32_t buses = pci_config_read(bus, slot, func, 0x18);
                uint8_t secondary_bus = (buses >> 8) & 0xFF;
                if (secondary_bus > bus && secondary_bus != 0) {
                    pci_scan_bus(secondary_bus);
                }
            }
        }
    }
}

// Initialize the PCI subsystem (to be called at kernel startup)
void pci_init(void) {
    kprintf("Initializing PCI subsystem...\n");
    pci_device_count = 0;
    pci_config_space_init();
    pci_scan_bus(0);
    kprintf("PCI: Found %d PCI device(s)\n", pci_device_count);
}

// Finder function for drivers to locate a specific device
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; ++i) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Returns the number of PCI devices discovered.
int pci_get_device_count(void) {
    return pci_device_count;
}

// Returns a pointer to the PCI device at the given index (or NULL if index is out of range).
pci_device_t* pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count) {
        return NULL;
    }
    return &pci_devices[index];
}
