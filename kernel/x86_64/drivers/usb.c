#include <drivers/usb.h>
#include <drivers/pci.h>
#include <printf.h>
#include <panic.h>
#include <io.h>
#include <mm/pmm.h>
#include <string.h>
#include <drivers/ehci.h>
#include <drivers/uhci.h>

/* Global USB device list and count */
usb_device_t usb_devices[MAX_USB_DEVICES];
int usb_device_count = 0;

/* Global array for all USB host controllers */
static usb_host_controller_t usb_controllers[8];
static int usb_hc_count = 0;

/* Global array for UHCI controllers (for interrupt handling) */
#define MAX_UHCI_CONTROLLERS 8
usb_host_controller_t uhci_controllers[MAX_UHCI_CONTROLLERS];
int num_uhci_controllers = 0;

/*
 * Allocate a 4KB-aligned page.
 */
void* usb_alloc_page(void) {
    void *page = kmalloca(4096, 4096);
    if (!page)
        panic("USB: Out of memory for controller structures");
    memset(page, 0, 4096);
    return page;
}

/*
 * Initialize a UHCI host controller.
 */
static int init_uhci_controller(pci_device_t *pci_dev) {
    uint32_t io_base = pci_dev->bar[4] & ~0xF;
    if (io_base == 0)
        return -1;

    usb_host_controller_t *hc = &usb_controllers[usb_hc_count++];
    hc->type = USB_HC_UHCI;
    hc->pci_dev = pci_dev;
    hc->io_base = io_base;

    hc->uhci.frame_list = usb_alloc_page();
    for (int i = 0; i < 1024; i++)
        hc->uhci.frame_list[i] = 0x00000001;

    hc->uhci.control_qh = kmalloca(sizeof(uhci_qh), 16);
    hc->uhci.control_qh->head = 0x00000001;
    hc->uhci.control_qh->element = 0x00000001;

    hc->uhci.frame_list[0] = (uint32_t)(uintptr_t)hc->uhci.control_qh | 0x2;

    outw(io_base + 0x00, 0x0004);
    for (volatile int d = 0; d < 10000; d++);
    outw(io_base + 0x00, 0x0000);

    outdw(io_base + 0x08, (uint32_t)(uintptr_t)hc->uhci.frame_list);
    outw(io_base + 0x06, 0x0000);

    outw(io_base + 0x00, 0x0041);

    uint8_t ports = 0;
    for (uint8_t p = 0; p < 8; p++) {
        if (inw(io_base + 0x10 + 2 * p) == 0xFFFF)
            break;
        ports++;
    }
    hc->num_ports = ports;

    kprintf("USB: Initialized UHCI controller on bus %d slot %d with %d ports\n",
            pci_dev->bus, pci_dev->slot, ports);

    // Register UHCI interrupt for this controller.
    uhci_register_interrupt(pci_dev);
    // Add this controller to the UHCI global array.
    if (num_uhci_controllers < MAX_UHCI_CONTROLLERS) {
        uhci_controllers[num_uhci_controllers++] = *hc;
    } else {
        kprintf("UHCI: Too many UHCI controllers!\n");
    }
    
    for (uint8_t p = 0; p < ports; p++) {
        uint16_t port_status = inw(io_base + 0x10 + 2 * p);
        if (port_status & 0x1) {
            outw(io_base + 0x10 + 2 * p, port_status | (1 << 9));
            for (volatile int d = 0; d < 10000; d++);
            outw(io_base + 0x10 + 2 * p, port_status & ~(1 << 9));
            for (volatile int d = 0; d < 5000; d++);
            outw(io_base + 0x10 + 2 * p, inw(io_base + 0x10 + 2 * p) | (1 << 2));
            uint8_t low_speed = (inw(io_base + 0x10 + 2 * p) & (1 << 8)) ? 1 : 0;
            usb_process_device_connect(hc, p, low_speed);
        }
    }
    return 0;
}

/* Stub functions for OHCI, EHCI, xHCI */
static int init_ohci_controller(pci_device_t *pci_dev) { return -1; }
static int init_ehci_controller(pci_device_t *pci_dev) { return -1; }
static int init_xhci_controller(pci_device_t *pci_dev) { return -1; }

/*
 * Unified control transfer helper.
 */
int usb_control_transfer(usb_host_controller_t *hc, uint8_t device_address,
                         usb_setup_packet_t *setup, void *buffer, int length) {
    int ret = -1;
    switch (hc->type) {
        case USB_HC_EHCI:
            ret = ehci_control_transfer(hc, device_address, setup, buffer, length);
            break;
        case USB_HC_UHCI:
            ret = uhci_control_transfer(hc, device_address, setup, buffer, length);
            break;
        default:
            kprintf("USB: Unsupported controller type %d\n", hc->type);
            ret = -1;
            break;
    }
    return ret;
}

/*
 * Process a new device connection on a root hub port.
 */
void usb_process_device_connect(usb_host_controller_t *hc, uint8_t port, uint8_t low_speed) {
    if (usb_device_count >= MAX_USB_DEVICES) {
        kprintf("USB: Max device limit reached, cannot enumerate port %d\n", port);
        return;
    }
    
    usb_device_t *dev = &usb_devices[usb_device_count++];
    dev->address = usb_device_count;
    dev->type = USB_DEVICE_UNKNOWN;
    dev->controller = hc->type;
    
    kprintf("USB: Device connected on controller %s port %d (low_speed=%d), assigned address %d\n",
            (hc->type == USB_HC_EHCI) ? "EHCI" :
            (hc->type == USB_HC_UHCI) ? "UHCI" : "Other",
            port, low_speed, dev->address);
    
    int ret = 0;
    uint8_t buf[18] = {0};
    usb_setup_packet_t setup;
    
    /* Get short Device Descriptor (8 bytes) */
    setup.bmRequestType = 0x80;
    setup.bRequest      = 6;
    setup.wValue        = 0x0100;
    setup.wIndex        = 0x0000;
    setup.wLength       = 8;
    ret = usb_control_transfer(hc, 0, &setup, buf, 8);
    if (ret != 0) {
        kprintf("USB: Failed to get short device descriptor (port %d)\n", port);
        return;
    }
    
    /* Get full Device Descriptor (18 bytes) */
    setup.wLength = 18;
    ret = usb_control_transfer(hc, 0, &setup, buf, 18);
    if (ret != 0) {
        kprintf("USB: Failed to get full device descriptor (port %d)\n", port);
        return;
    }
    
    kprintf("USB: Device Descriptor for address %d:", dev->address);
    for (int i = 0; i < 18; i++) {
        kprintf(" %02x", buf[i]);
    }
    kprintf("\n");
    
    /* Set Device Address */
    setup.bmRequestType = 0x00;
    setup.bRequest      = 5;
    setup.wValue        = dev->address;
    setup.wIndex        = 0;
    setup.wLength       = 0;
    ret = usb_control_transfer(hc, 0, &setup, NULL, 0);
    if (ret != 0) {
        kprintf("USB: Failed to set device address (port %d)\n", port);
        return;
    }
    
    /* Get Configuration Descriptor header (9 bytes) */
    setup.bmRequestType = 0x80;
    setup.bRequest      = 6;
    setup.wValue        = 0x0200;
    setup.wIndex        = 0;
    setup.wLength       = 9;
    uint8_t conf_buf[256] = {0};
    ret = usb_control_transfer(hc, dev->address, &setup, conf_buf, 9);
    if (ret != 0) {
        kprintf("USB: Failed to get configuration descriptor header (port %d)\n", port);
        return;
    }
    uint16_t total_len = conf_buf[2] | (conf_buf[3] << 8);
    
    /* Get full Configuration Descriptor */
    setup.wLength = total_len;
    ret = usb_control_transfer(hc, dev->address, &setup, conf_buf, total_len);
    if (ret != 0) {
        kprintf("USB: Failed to get full configuration descriptor (port %d)\n", port);
        return;
    }
    
    /* Parse Configuration Descriptor for a HID Keyboard interface */
    for (int i = 0; i < total_len; ) {
        if (conf_buf[i+1] == 0x04) {
            uint8_t iface_class = conf_buf[i+5];
            uint8_t iface_subclass = conf_buf[i+6];
            uint8_t iface_protocol = conf_buf[i+7];
            if (iface_class == 0x03 && iface_subclass == 0x01 && iface_protocol == 0x01) {
                dev->type = USB_DEVICE_KEYBOARD;
                break;
            }
        }
        int desc_len = conf_buf[i];
        if (desc_len == 0)
            break;
        i += desc_len;
    }
    
    /* Set Configuration (assume configuration 1) */
    setup.bmRequestType = 0x00;
    setup.bRequest      = 9;
    setup.wValue        = 1;
    setup.wIndex        = 0;
    setup.wLength       = 0;
    ret = usb_control_transfer(hc, dev->address, &setup, NULL, 0);
    if (ret != 0) {
        kprintf("USB: Failed to set configuration (port %d)\n", port);
        return;
    }
    
    kprintf("USB: Device enumerated successfully. Address: %d, Type: %s, Controller: %s\n",
            dev->address,
            (dev->type == USB_DEVICE_KEYBOARD) ? "HID Keyboard" : "Unknown USB Device",
            (hc->type == USB_HC_EHCI) ? "EHCI" : (hc->type == USB_HC_UHCI) ? "UHCI" : "Other");
}

/*
 * Initialize the USB subsystem by scanning PCI devices for USB host controllers.
 */
void usb_init(void) {
    kprintf("Initializing USB subsystem...\n");
    usb_device_count = 0;
    usb_hc_count = 0;
    int found = 0;

    for (int i = 0; i < pci_get_device_count(); i++) {
        pci_device_t *dev = pci_get_device(i);
        if (dev->class_code == 0x0C && dev->subclass == 0x03) {
            uint8_t interface = dev->prog_if;
            int res = -1;
            kprintf("INTERFACE [%llu]\n", interface);
            if (interface == 0x00) {
                uhci_register_interrupt(dev);
                res = init_uhci_controller(dev);
            }
            else if (interface == 0x10) {
                res = init_ohci_controller(dev);
            }
            else if (interface == 0x20)
                res = ehci_init_controller(dev, &usb_controllers[usb_hc_count]);
            else if (interface == 0x30)
                res = init_xhci_controller(dev);
            if (res == 0)
                found++;
        }
    }
    if (!found) {
        kprintf("USB: No USB host controllers found on PCI.\n");
    }
}

/*
 * USB polling routine.
 */
void usb_poll(void) {
    // Optional: Poll for status changes if needed.
}

/*
 * Read from a USB device.
 */
int usb_read(usb_device_t *dev, void *buffer, int length) {
    if (dev->type == USB_DEVICE_KEYBOARD) {
        uint8_t report[8] = {0};
        report[2] = 0x04;
        int to_copy = (length < 8) ? length : 8;
        memcpy(buffer, report, to_copy);
        return to_copy;
    }
    return 0;
}

/* Helper functions for printing device info */
const char* usb_device_type_name(usb_device_type_t type) {
    switch (type) {
        case USB_DEVICE_KEYBOARD:
            return "USB Keyboard";
        case USB_DEVICE_UNKNOWN:
        default:
            return "Unknown USB Device";
    }
}

const char* usb_host_controller_name(usb_hc_type_t type) {
    switch (type) {
        case USB_HC_UHCI:
            return "UHCI";
        case USB_HC_EHCI:
            return "EHCI";
        case USB_HC_OHCI:
            return "OHCI";
        case USB_HC_XHCI:
            return "xHCI";
        default:
            return "Unknown Controller";
    }
}

void usb_print_devices(void) {
    kprintf("USB: %d device(s) discovered:\n", usb_device_count);
    for (int i = 0; i < usb_device_count; i++) {
        usb_device_t *dev = &usb_devices[i];
        kprintf("  Device %d: Address = %d, Type = %s, Controller = %s\n",
                i + 1, dev->address, usb_device_type_name(dev->type), usb_host_controller_name(dev->controller));
    }
}
