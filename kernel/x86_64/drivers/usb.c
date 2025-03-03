#include <drivers/usb.h>
#include <drivers/pci.h>
#include <drivers/ehci.h>
#include <drivers/uhci.h>
#include <printf.h>
#include <panic.h>
#include <io.h>
#include <mm/pmm.h>
#include <string.h>

/* 
 * Global USB device list and count
 */
usb_device_t usb_devices[MAX_USB_DEVICES];
int          usb_device_count = 0;

/*
 * Global array for all USB host controllers
 */
usb_host_controller_t usb_controllers[8];
int                  usb_hc_count = 0;

/*
 * Helper for allocating a page (4KB) for USB driver structures.
 * This uses your existing kernel memory subsystem; update if needed.
 */
void* usb_alloc_page(void) {
    // Example uses 'kmalloca' to allocate and align to 4096
    // If your OS uses 'pmm_alloc' or something else, adapt accordingly
    extern void *kmalloca(size_t size, size_t align);
    void *page = kmalloca(4096, 4096);
    if (!page) {
        panic("USB: Out of memory for controller structures");
    }
    memset(page, 0, 4096);
    return page;
}

static void enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func) {
    // Read current PCI Command register
    uint16_t cmd = pci_read16(bus, slot, func, 0x04);

    // Set the "Bus Master Enable" bit (bit 2) and Memory Space bit (bit 1)
    cmd |= (1 << 2) | (1 << 1);

    // Write it back
    pci_write16(bus, slot, func, 0x04, cmd);
    kprintf("USB: Enabled PCI bus mastering; cmd=0x%04x (bus=%u,slot=%u,func=%u)\n",
            cmd, bus, slot, func);
}

/*
 * Initialize a UHCI controller
 */
static int init_uhci_controller(pci_device_t *pci_dev)
{
    enable_bus_mastering(pci_dev->bus, pci_dev->slot, pci_dev->function);

    uint32_t io_base = pci_dev->bar[4] & ~0xF;
    if (io_base == 0) {
        return -1;
    }

    usb_host_controller_t *hc = &usb_controllers[usb_hc_count++];
    memset(hc, 0, sizeof(*hc));

    hc->type    = USB_HC_UHCI;
    hc->pci_dev = pci_dev;
    hc->io_base = io_base;

    // Basic UHCI init
    hc->uhci.frame_list = usb_alloc_page();
    for (int i = 0; i < 1024; i++) {
        hc->uhci.frame_list[i] = 0x00000001;
    }
    hc->uhci.control_qh = (uhci_qh*)kmalloca(sizeof(uhci_qh), 16);
    hc->uhci.control_qh->head    = 0x00000001;
    hc->uhci.control_qh->element = 0x00000001;

    hc->uhci.frame_list[0] = ((uint32_t)(uintptr_t)hc->uhci.control_qh) | 0x2;

    // Reset then run controller
    outw(io_base + 0x00, 0x0004);
    for (volatile int d = 0; d < 10000; d++);
    outw(io_base + 0x00, 0x0000);

    outdw(io_base + 0x08, (uint32_t)(uintptr_t)hc->uhci.frame_list);
    outw(io_base + 0x06, 0x0000);
    outw(io_base + 0x00, 0x0041);

    // Count ports
    uint8_t ports = 0;
    for (uint8_t p = 0; p < 8; p++) {
        if (inw(io_base + 0x10 + 2 * p) == 0xFFFF) {
            break;
        }
        ports++;
    }
    hc->num_ports = ports;
    kprintf("USB: Initialized UHCI controller on bus %u slot %u with %u ports\n",
            pci_dev->bus, pci_dev->slot, ports);

    // Example of hooking interrupts in UHCI
    extern int num_uhci_controllers;
    extern uhci_controller_t uhci_controllers[];
    if (num_uhci_controllers < MAX_UHCI_CONTROLLERS) {
        uhci_controller_t *uc = &uhci_controllers[num_uhci_controllers++];
        memset(uc, 0, sizeof(*uc));
        uc->hc  = hc;
        uc->irq = pci_dev->irq;
        uhci_register_interrupt(uc);

        // Check if anything is already plugged in
        for (uint8_t p = 0; p < ports; p++) {
            uint16_t port_status = inw(io_base + 0x10 + 2 * p);
            if (port_status & 0x1) {  // device connected
                // Reset
                outw(io_base + 0x10 + 2 * p, port_status | (1 << 9));
                for (volatile int d = 0; d < 10000; d++);
                outw(io_base + 0x10 + 2 * p, port_status & ~(1 << 9));
                for (volatile int d = 0; d < 5000; d++);

                // Enable port
                outw(io_base + 0x10 + 2 * p, inw(io_base + 0x10 + 2 * p) | (1 << 2));
                uint8_t low_speed = (inw(io_base + 0x10 + 2 * p) & (1 << 8)) ? 1 : 0;

                // Kick off enumeration
                usb_process_device_connect(hc, p, low_speed);
            }
        }
    } else {
        kprintf("UHCI: Too many UHCI controllers, can't register interrupt!\n");
    }
    return 0;
}

/*
 * Initialize an OHCI controller (stub)
 */
static int init_ohci_controller(pci_device_t *pci_dev) {
    return -1; // Stub
}

/*
 * Initialize an EHCI controller
 * Example: We rely on an external 'ehci_init_controller()' in ehci.c
 */
static int init_ehci_wrapper(pci_device_t *dev) {
    // We pass the next available usb_host_controller_t to the EHCI driver.
    usb_host_controller_t *hc = &usb_controllers[usb_hc_count];
    memset(hc, 0, sizeof(*hc));
    hc->type    = USB_HC_EHCI;
    hc->pci_dev = dev;

    int ret = ehci_init_controller(dev, hc);
    if (ret == 0) {
        usb_hc_count++;
    }
    return ret;
}

/*
 * We replace the old stub with a real xHCI initialization from xhci.c
 * but here's a forward reference in case we need it
 */
// extern int init_xhci_controller(pci_device_t *dev);

/*
 * The unified control transfer helper
 */
int usb_control_transfer(usb_host_controller_t *hc, uint8_t device_address,
                         usb_setup_packet_t *setup, void *buffer, int length) {
    int ret = -1;
    switch (hc->type) {
        case USB_HC_UHCI:
            // If you have a dedicated UHCI control transfer function:
            ret = uhci_control_transfer(hc, device_address, setup, buffer, length);
            break;
        case USB_HC_EHCI:
            ret = ehci_control_transfer(hc, device_address, setup, buffer, length);
            break;
        case USB_HC_XHCI:
            ret = xhci_control_transfer(hc, device_address, setup, buffer, length);
            break;
        default:
            kprintf("USB: Unsupported controller type %d\n", hc->type);
            break;
    }
    return ret;
}

/*
 * Called when a device is connected at a root hub port.
 * This function enumerates the device. We do basic steps:
 *   1) Get 8 bytes of device descriptor
 *   2) Get full device descriptor (18 bytes)
 *   3) Assign an address
 *   4) Get config descriptor
 *   5) Set config
 */
void usb_process_device_connect(usb_host_controller_t *hc, uint8_t port, uint8_t low_speed) {
    if (usb_device_count >= MAX_USB_DEVICES) {
        kprintf("USB: Max device limit reached, cannot enumerate port %d\n", port);
        return;
    }

    // We'll assign the device an address = usb_device_count + 1
    // (0 is reserved for default address phase)
    uint8_t new_address = (uint8_t)(usb_device_count + 1);

    usb_device_t *dev = &usb_devices[usb_device_count++];
    memset(dev, 0, sizeof(*dev));
    dev->address    = new_address;
    dev->type       = USB_DEVICE_UNKNOWN;
    dev->controller = hc->type;

    kprintf("USB: Device connected on controller %s port %d (low_speed=%d), assigned address %d\n",
        usb_host_controller_name(hc->type), port, low_speed, dev->address);

    int ret = 0;
    uint8_t buf[18] = {0};

    // 1) Get 8 bytes of device descriptor
    usb_setup_packet_t setup;
    memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x80; // Device-to-host standard request
    setup.bRequest      = 6;    // GET_DESCRIPTOR
    setup.wValue        = 0x0100; // Device descriptor
    setup.wIndex        = 0;
    setup.wLength       = 8;

    ret = usb_control_transfer(hc, 0, &setup, buf, 8);
    if (ret) {
        kprintf("USB: Failed to get short device descriptor (port %d)\n", port);
        return;
    }

    // 2) Get full device descriptor (18 bytes)
    setup.wLength = 18;
    ret = usb_control_transfer(hc, 0, &setup, buf, 18);
    if (ret) {
        kprintf("USB: Failed to get full device descriptor (port %d)\n", port);
        return;
    }

    kprintf("USB: Device descriptor (addr=0 - enumerating):");
    for (int i = 0; i < 18; i++) {
        kprintf(" %02x", buf[i]);
    }
    kprintf("\n");

    // 3) Set device address
    setup.bmRequestType = 0x00;
    setup.bRequest      = 5;   // SET_ADDRESS
    setup.wValue        = new_address;
    setup.wIndex        = 0;
    setup.wLength       = 0;

    ret = usb_control_transfer(hc, 0, &setup, NULL, 0);
    if (ret) {
        kprintf("USB: Failed to set device address (port %d)\n", port);
        return;
    }

    // 4) Get configuration descriptor header
    uint8_t conf_buf[256] = {0};
    setup.bmRequestType = 0x80;
    setup.bRequest      = 6;   // GET_DESCRIPTOR
    setup.wValue        = 0x0200; // Configuration descriptor
    setup.wIndex        = 0;
    setup.wLength       = 9;

    ret = usb_control_transfer(hc, new_address, &setup, conf_buf, 9);
    if (ret) {
        kprintf("USB: Failed to get configuration descriptor header (port %d)\n", port);
        return;
    }

    uint16_t total_len = conf_buf[2] | (conf_buf[3] << 8);
    if (total_len > sizeof(conf_buf)) {
        kprintf("USB: Config descriptor too large (%u), truncating\n", total_len);
        total_len = sizeof(conf_buf);
    }

    // 5) Get full config descriptor
    setup.wLength = total_len;
    ret = usb_control_transfer(hc, new_address, &setup, conf_buf, total_len);
    if (ret) {
        kprintf("USB: Failed to get full config descriptor (port %d)\n", port);
        return;
    }

    // Quick parse for a HID keyboard interface
    for (int i = 0; i < total_len; ) {
        if (conf_buf[i+1] == 0x04) { // Interface descriptor
            uint8_t iface_class = conf_buf[i+5];
            uint8_t iface_subclass = conf_buf[i+6];
            uint8_t iface_protocol = conf_buf[i+7];
            if (iface_class == 0x03 && iface_subclass == 0x01 && iface_protocol == 0x01) {
                dev->type = USB_DEVICE_KEYBOARD;
                break;
            }
        }
        int desc_len = conf_buf[i];
        if (desc_len == 0) {
            break;
        }
        i += desc_len;
    }

    // 6) Set configuration (assume configuration = 1)
    setup.bmRequestType = 0x00;
    setup.bRequest      = 9; // SET_CONFIGURATION
    setup.wValue        = 1;
    setup.wIndex        = 0;
    setup.wLength       = 0;

    ret = usb_control_transfer(hc, new_address, &setup, NULL, 0);
    if (ret) {
        kprintf("USB: Failed to set configuration (port %d)\n", port);
        return;
    }

    kprintf("USB: Device enumerated. Address=%d, Type=%s, Controller=%s\n",
        dev->address,
        usb_device_type_name(dev->type),
        usb_host_controller_name(dev->controller));
}

/*
 * Initialize USB subsystem: scan PCI for USB controllers, init them
 */
void usb_init(void) {
    kprintf("Initializing USB subsystem...\n");
    usb_device_count = 0;
    usb_hc_count     = 0;
    int found        = 0;

    extern int pci_get_device_count(void);
    extern pci_device_t* pci_get_device(int index);

    int dev_count = pci_get_device_count();
    for (int i = 0; i < dev_count; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (dev->class_code == 0x0C && dev->subclass == 0x03) {
            uint8_t interface = dev->prog_if;
            int res = -1;
            if (interface == 0x00) {   // UHCI
                res = init_uhci_controller(dev);
            } else if (interface == 0x10) {  // OHCI
                res = init_ohci_controller(dev);
            } else if (interface == 0x20) {  // EHCI
                res = init_ehci_wrapper(dev);
            } else if (interface == 0x30) {  // xHCI
                res = init_xhci_controller(dev);
            }
            if (res == 0) {
                found++;
            }
        }
    }
    if (!found) {
        kprintf("USB: No USB host controllers found.\n");
    }
}

/*
 * USB polling routine
 */
void usb_poll(void) {
    // Some drivers (UHCI) might require periodic checks for TD completion, etc.
    // EHCI/xHCI are typically interrupt-driven, so maybe not required.
}

/*
 * Read from a USB device. Very basic example.
 * For a real driver, we'd do an interrupt or bulk transfer to poll the device.
 */
int usb_read(usb_device_t *dev, void *buffer, int length) {
    if (dev->type == USB_DEVICE_KEYBOARD) {
        // Example just fakes data for demonstration
        uint8_t report[8] = {0};
        // Put 'A' scancode or something
        report[2] = 0x04;  // Key 'a' scancode in HID usage
        int to_copy = (length < 8) ? length : 8;
        memcpy(buffer, report, to_copy);
        return to_copy;
    }
    return 0;
}

const char* usb_device_type_name(usb_device_type_t type) {
    switch (type) {
        case USB_DEVICE_KEYBOARD: return "Keyboard";
        case USB_DEVICE_UNKNOWN:
        default:                  return "Unknown";
    }
}

const char* usb_host_controller_name(usb_hc_type_t type) {
    switch (type) {
        case USB_HC_UHCI: return "UHCI";
        case USB_HC_OHCI: return "OHCI";
        case USB_HC_EHCI: return "EHCI";
        case USB_HC_XHCI: return "xHCI";
        default:          return "UnknownHC";
    }
}

void usb_print_devices(void) {
    kprintf("USB: %d devices enumerated:\n", usb_device_count);
    for (int i = 0; i < usb_device_count; i++) {
        usb_device_t *ud = &usb_devices[i];
        kprintf("  Dev %2d: Address=%2d, Type=%s, HC=%s\n",
            i+1, ud->address, usb_device_type_name(ud->type),
            usb_host_controller_name(ud->controller));
    }
}
