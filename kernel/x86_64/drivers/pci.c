#include <drivers/pci.h>
#include <io.h>
#include <printf.h>
#include <acpi/acpi.h>
#include <panic.h>
#include <mm/pgtable.h>
#include <mm/pmm.h>
#include <stdbool.h>

static uint64_t pci_get_bar_size(uint16_t bus, uint16_t slot, uint16_t func, int bar_index);

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
    {0x8086, 0x100E, "Intel", "82540EM Gigabit Ethernet"},
    {0x8086, 0x1533, "Intel", "I210 Gigabit Network Connection"},
    {0x8086, 0x10D3, "Intel", "82574L Gigabit Network Connection"},
    {0x8086, 0x15B8, "Intel", "Ethernet Connection I219-V"},
    {0x8086, 0x29c0, "Intel", "82G33/G31/P35/P31 Express DRAM Controller"},
    {0x8086, 0x3E98, "Intel", "UHD Graphics 630"},
    {0x8086, 0x5912, "Intel", "HD Graphics 630"},
    {0x8086, 0x9BC5, "Intel", "Iris Xe Graphics"},
    {0x8086, 0x8C31, "Intel", "USB 3.0 eXtensible Host Controller"},
    {0x8086, 0xA36D, "Intel", "Cannon Lake PCH USB 3.1 xHCI Controller"},
    {0x8086, 0x24cd, "Intel", "82801DB/DBM (ICH4/ICH4-M) USB2 EHCI Controller"},
    {0x8086, 0x2918, "Intel", "82801IB (ICH9) LPC Interface Controller"},
    {0x8086, 0x2930, "Intel", "82801I (ICH9 Family) SMBus Controller"},
    {0x1b36, 0x000d, "Red Hat, Inc", "QEMU XHCI Host Controller"},
    {0x8086, 0x2922, "Intel", "ICH9 AHCI SATA Controller"},
    {0x8086, 0x8C02, "Intel", "8 Series/C220 Series AHCI Controller"},
    {0x8086, 0x0953, "Intel", "SSD 750 Series NVMe Controller"},
    {0x1002, 0x67DF, "AMD", "Radeon RX 580"},
    {0x1002, 0x731F, "AMD", "Radeon RX 5700 XT"},
    {0x1002, 0x73BF, "AMD", "Radeon RX 6800 XT"},
    {0x1022, 0x2000, "AMD", "PCnet-PCI Ethernet Adapter"},
    {0x1022, 0x145C, "AMD", "X370 USB 3.1 Controller"},
    {0x1022, 0x7901, "AMD", "SB7x0/SB8x0/SB9x0 SATA Controller"},
    {0x10DE, 0x1C82, "NVIDIA", "GeForce GTX 1050 Ti"},
    {0x10DE, 0x2184, "NVIDIA", "GeForce RTX 3060"},
    {0x10DE, 0x2204, "NVIDIA", "GeForce RTX 3080 Ti"},
    {0x10DE, 0x2482, "NVIDIA", "GeForce RTX 4090"},
    {0x10EC, 0x8139, "Realtek", "RTL-8139 Fast Ethernet"},
    {0x10EC, 0x8168, "Realtek", "RTL8111/8168/8411 PCI Express Gigabit Ethernet"},
    {0x10EC, 0x8125, "Realtek", "RTL8125 2.5Gb Ethernet"},
    {0x14E4, 0x165F, "Broadcom", "NetXtreme BCM5719 Gigabit Ethernet"},
    {0x14E4, 0x16A1, "Broadcom", "BCM57416 NetXtreme-E 10Gb Ethernet"},
    {0x14E4, 0x43A0, "Broadcom", "BCM4360 802.11ac Wireless Network Adapter"},
    {0x14E4, 0x43B1, "Broadcom", "BCM4352 802.11ac Wireless Network Adapter"},
    {0x168C, 0x003E, "Qualcomm Atheros", "QCA6174 802.11ac Wireless Network Adapter"},
    {0x168C, 0x0042, "Qualcomm Atheros", "QCA9377 802.11ac Wireless Network Adapter"},
    {0x1B21, 0x0612, "ASMedia", "ASM1062 SATA Controller"},
    {0x1B21, 0x1242, "ASMedia", "ASM2142 USB 3.1 Host Controller"},
    {0x1B4B, 0x9235, "Marvell", "88SE9235 PCIe SATA Controller"},
    {0x15AD, 0x0405, "VMware", "VMXNET3 Ethernet Adapter"},
    {0x15AD, 0x0770, "VMware", "SVGA II Adapter"},
    {0x1AF4, 0x1000, "VirtIO", "VirtIO Network Adapter"},
    {0x1AF4, 0x1042, "VirtIO", "VirtIO Block Device"},
    {0x1AF4, 0x1052, "VirtIO", "VirtIO GPU Device"},
    {0x1033, 0x0194, "NEC", "USB 3.0 Host Controller"},
    {0x1B6F, 0x7052, "Etron", "USB 3.0 xHCI Controller"},
    {0x1D6A, 0x07B1, "Google", "Titan Security Key"},
    {0x8086, 0xA170, "Intel", "HD Audio Controller"},
    {0x1102, 0x0004, "Creative", "Sound Blaster Audigy"},
    {0x10EC, 0x0887, "Realtek", "ALC887 HD Audio Codec"},
    {0x144D, 0xA808, "Samsung", "NVMe SSD Controller (Samsung 970 EVO)"},
    {0x1C5C, 0x1327, "Phison", "E12 NVMe SSD Controller"},
    {0, 0, "Unknown", "Unknown"}
};

static const char* pci_lookup_vendor(uint16_t vid) {
    for (int i = 0; pci_known_ids[i].vendor; ++i)
        if (pci_known_ids[i].vendor == vid)
            return pci_known_ids[i].vendor_name;
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
    if (pci_mcfg_base) {
        // Read the actual PCIe MMCONFIG base address from the MCFG table at offset 0x2C
        pci_mcfg_base = *(uint64_t *)(pci_mcfg_base + 0x2C);
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

uint8_t pci_read8(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset) {
    uint8_t aligned_offset = offset & 0xFC;  // align to 32-bit boundary
    uint32_t value = pci_config_read(bus, slot, function, aligned_offset);
    int shift = (offset & 0x3) * 8;          // offset within the dword (0,1,2,3) * 8 bits
    return (uint8_t)((value >> shift) & 0xFF);
}

void pci_write8(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset, uint8_t data) {
    uint8_t aligned_offset = offset & 0xFC;  // align to 32-bit boundary
    uint32_t oldval = pci_config_read(bus, slot, function, aligned_offset);
    int shift = (offset & 0x3) * 8;          // determine which byte to update
    uint32_t mask = 0xFF << shift;           // mask for the target byte
    uint32_t newval = (oldval & ~mask) | ((uint32_t)data << shift);
    pci_config_write(bus, slot, function, aligned_offset, newval);
}

uint16_t pci_read16(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset) {
    uint8_t aligned_offset = offset & 0xFC;
    uint32_t value = pci_config_read(bus, slot, function, aligned_offset);
    int shift = (offset & 2) * 8;
    return (uint16_t)((value >> shift) & 0xFFFF);
}

void pci_write16(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset, uint16_t data) {
    uint8_t aligned_offset = offset & 0xFC;
    uint32_t oldval = pci_config_read(bus, slot, function, aligned_offset);
    int shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFF << shift;
    uint32_t newval = (oldval & ~mask) | ((data & 0xFFFF) << shift);
    pci_config_write(bus, slot, function, aligned_offset, newval);
}

uint32_t pci_read32(uint16_t bus, uint16_t slot, uint16_t function, uint8_t offset) {
    return pci_config_read(bus, slot, function, offset);
}

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

/*
 * pci_resource_len - Return the length in bytes of a given PCI device BAR.
 *
 * @dev: Pointer to the pci_device_t structure.
 * @bar_index: The BAR index (0 to 5).
 *
 * Returns: The size (in bytes) of the BAR as reported by the device,
 *          or 0 if the BAR is unused or the index is invalid.
 */
uint64_t pci_resource_len(pci_device_t *dev, int bar_index)
{
    if (!dev || bar_index < 0 || bar_index >= 6) {
        kprintf("pci_resource_len: Invalid device or BAR index %d\n", bar_index);
        return 0;
    }

    uint64_t size = pci_get_bar_size(dev->bus, dev->slot, dev->function, bar_index);
    if (size == 0)
        kprintf("pci_resource_len: BAR[%d] reports size 0\n", bar_index);

    return size;
}

/*
 * pci_resource_start - Return the physical start address of a PCI device BAR.
 *
 * @dev: Pointer to the pci_device_t structure.
 * @bar_index: The BAR index (0 to 5).
 *
 * Returns: The physical base address of the BAR as stored in the device's
 *          BAR array, or 0 if the index is invalid.
 */
uintptr_t pci_resource_start(pci_device_t *dev, int bar_index)
{
    if (!dev || bar_index < 0 || bar_index >= 6) {
        kprintf("pci_resource_start: Invalid device or BAR index %d\n", bar_index);
        return 0;
    }

    return dev->bar[bar_index];
}

// Improved BAR size calculation with support for 64-bit BARs.
static uint64_t pci_get_bar_size(uint16_t bus, uint16_t slot, uint16_t func, int bar_index) {
    uint32_t bar_offset = 0x10 + bar_index * 4;
    uint32_t original_value = pci_config_read(bus, slot, func, bar_offset);
    pci_config_write(bus, slot, func, bar_offset, 0xFFFFFFFF);
    uint32_t size_mask = pci_config_read(bus, slot, func, bar_offset);
    pci_config_write(bus, slot, func, bar_offset, original_value);

    if (size_mask == 0 || size_mask == 0xFFFFFFFF) {
        return 0; // Unused BAR
    }

    uint64_t size = ~(size_mask & 0xFFFFFFF0) + 1;

    // Check if it's a 64-bit BAR (type bits 2:1 equal to 10, which gives 0x4 when masked with 0x6)
    if ((original_value & 0x6) == 0x4) {  
        uint32_t high_bar_offset = 0x10 + (bar_index + 1) * 4;
        uint32_t original_high_value = pci_config_read(bus, slot, func, high_bar_offset);
        pci_config_write(bus, slot, func, high_bar_offset, 0xFFFFFFFF);
        uint32_t size_mask_high = pci_config_read(bus, slot, func, high_bar_offset);
        pci_config_write(bus, slot, func, high_bar_offset, original_high_value);
        uint64_t full_size_mask = ((uint64_t)size_mask_high << 32) | size_mask;
        size = ~(full_size_mask & 0xFFFFFFFFFFFFFFF0) + 1;
    }

    return size;
}

// Change this function from static to public so it can be used by other modules.
int pci_find_capability(uint16_t bus, uint16_t slot, uint16_t func, uint8_t cap_id) {
    // First, check if the device supports capabilities (bit 4 in the Status register).
    uint16_t status = pci_read16(bus, slot, func, 0x06);
    if (!(status & 0x10))
        return 0; // No capabilities

    // The pointer to the first capability structure is at offset 0x34.
    uint8_t cap_ptr = pci_config_read(bus, slot, func, 0x34) & 0xFF;
    while (cap_ptr) {
        uint8_t current_cap = pci_config_read(bus, slot, func, cap_ptr) & 0xFF;
        if (current_cap == cap_id)
            return cap_ptr;
        cap_ptr = pci_config_read(bus, slot, func, cap_ptr + 1) & 0xFF;
    }
    return 0; // Not found
}

static void pci_probe_function(uint16_t bus, uint16_t slot, uint16_t func) {
    uint32_t id_reg = pci_config_read(bus, slot, func, 0x00);
    uint16_t vendor_id = id_reg & 0xFFFF;
    if (vendor_id == 0xFFFF) {
        return; // Device doesn't exist
    }
    uint16_t device_id = (id_reg >> 16) & 0xFFFF;
    uint32_t class_reg = pci_config_read(bus, slot, func, 0x08);
    uint8_t class_code = (class_reg >> 24) & 0xFF;
    uint8_t subclass = (class_reg >> 16) & 0xFF;
    uint8_t prog_if = (class_reg >> 8) & 0xFF;
    uint32_t hdr_reg = pci_config_read(bus, slot, func, 0x0C);
    uint8_t header_type = (hdr_reg >> 16) & 0xFF;

    // Create a new PCI device entry if there's room.
    if (pci_device_count < PCI_MAX_DEVICES) {
        pci_device_t *dev = &pci_devices[pci_device_count];
        dev->bus = bus;
        dev->slot = slot;
        dev->function = func;
        dev->vendor_id = vendor_id;
        dev->device_id = device_id;
        dev->class_code = class_code;
        dev->subclass = subclass;
        dev->prog_if = prog_if;
        dev->header_type = header_type;

        // Read the Interrupt Line register (IRQ number)
        uint32_t irq_reg = pci_config_read(bus, slot, func, 0x3C);
        uint8_t irq_line = irq_reg & 0xFF; // Only the lower 8 bits store the IRQ line

        // Store it in the PCI device structure
        dev->irq = irq_line;

        kprintf("    IRQ=0x%02x\n", irq_line);

        // Initialize BARs to 0.
        for (int i = 0; i < 6; i++) {
            dev->bar[i] = 0;
        }
        pci_device_count++;
    } else {
        kprintf("PCI: Device count exceeded maximum limit\n");
        return;
    }

    // Determine number of BARs based on header type
    int max_bars = ((header_type & 0x7F) == 0x00) ? 6 : 2;  // Type 0 = 6 BARs, Type 1 = 2 BARs

    for (int bar = 0; bar < max_bars; ++bar) {
        uint32_t bar_offset = 0x10 + bar * 4;
        uint32_t bar_value = pci_config_read(bus, slot, func, bar_offset);
        if (bar_value == 0 || bar_value == 0xFFFFFFFF) {
            continue; // Unused BAR
        }

        bool is_mmio = !(bar_value & 0x1);
        uint64_t bar_addr = bar_value & (is_mmio ? 0xFFFFFFF0 : 0xFFFFFFFC);
        uint64_t bar_size = pci_get_bar_size(bus, slot, func, bar);

        if (bar_size == 0) {
            continue;  // Skip if size is 0
        }

        if (is_mmio) {
            uint64_t full_bar = bar_addr;
            // Check for 64-bit BARs
            if ((bar_value & 0x6) == 0x4) {  // 64-bit BAR detected
                uint32_t bar_high = pci_config_read(bus, slot, func, 0x10 + (bar + 1) * 4);
                full_bar |= ((uint64_t)bar_high << 32);
                pci_devices[pci_device_count - 1].bar[bar] = full_bar;
                kprintf("PCI: BAR[%d] = 0x%016lx (MMIO, 64-bit) Size=0x%lx\n", bar, full_bar, bar_size);
                bar++;  // Skip the next BAR as it's part of the 64-bit pair
                continue;
            } else {
                pci_devices[pci_device_count - 1].bar[bar] = bar_addr;
                kprintf("PCI: BAR[%d] = 0x%016lx (MMIO) Size=0x%lx\n", bar, bar_addr, bar_size);
            }
        } else {
            pci_devices[pci_device_count - 1].bar[bar] = bar_addr;
            kprintf("PCI: BAR[%d] = 0x%08x (I/O Space) Size=0x%lx\n", bar, (uint32_t)bar_addr, bar_size);
        }
    }

    // Handle Expansion ROM BAR at offset 0x30.
    uint32_t rom_bar = pci_config_read(bus, slot, func, 0x30);
    if (rom_bar && rom_bar != 0xFFFFFFFF) {
        // Enable the ROM by setting bit 0
        pci_config_write(bus, slot, func, 0x30, rom_bar | 0x1);
        uint32_t original_rom_bar = rom_bar;
        pci_config_write(bus, slot, func, 0x30, 0xFFFFFFFE);
        uint32_t rom_size_mask = pci_config_read(bus, slot, func, 0x30);
        pci_config_write(bus, slot, func, 0x30, original_rom_bar);
        if (rom_size_mask && rom_size_mask != 0xFFFFFFFE) {
            uint32_t rom_size = ~(rom_size_mask & 0xFFFFFFFE) + 1;
            kprintf("PCI: Expansion ROM BAR = 0x%08x Size=0x%x\n", rom_bar, rom_size);
        }
    }

    // Enable memory/IO decoding and bus mastering.
    uint32_t cmd_reg = pci_config_read(bus, slot, func, 0x04);
    cmd_reg |= 0x0007;  // Enable I/O space, memory space, and bus master.
    pci_config_write(bus, slot, func, 0x04, cmd_reg);

    kprintf("PCI: [%02x:%02x.%x] Vendor ID: 0x%04x, Device ID: 0x%04x\n",
            bus, slot, func, vendor_id, device_id);
    kprintf("    Class=0x%02x Subclass=0x%02x ProgIF=0x%02x\n",
            class_code, subclass, prog_if);
}

static void pci_scan_bus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; ++slot) {
        uint32_t id = pci_config_read(bus, slot, 0, 0x00);
        uint16_t vendor = id & 0xFFFF;
        if (!pci_device_exists(vendor))
            continue;
        uint8_t header_type = (pci_config_read(bus, slot, 0, 0x0C) >> 16) & 0xFF;
        int func_count = (header_type & 0x80) ? PCI_MAX_FUNCTIONS : 1;
        for (uint8_t func = 0; func < func_count; ++func) {
            uint32_t func_id = pci_config_read(bus, slot, func, 0x00);
            uint16_t vend = func_id & 0xFFFF;
            if (!pci_device_exists(vend))
                continue;
            pci_probe_function(bus, slot, func);
            // If this function is a PCI-PCI bridge, scan its secondary bus.
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

void pci_init(void) {
    kprintf("Initializing PCI subsystem...\n");
    pci_device_count = 0;
    pci_config_space_init();
    pci_scan_bus(0);
    kprintf("PCI: Found %d PCI device(s)\n", pci_device_count);
}

pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; ++i) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id)
            return &pci_devices[i];
    }
    return NULL;
}

int pci_get_device_count(void) {
    return pci_device_count;
}

pci_device_t* pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count)
        return NULL;
    return &pci_devices[index];
}
