#ifndef USB_H
#define USB_H

#include <stdint.h>
#include <drivers/pci.h>

#define MAX_USB_DEVICES 16

/*
 * EHCI Queue Head (EHCI QH) Structure
 *
 * This structure is used by EHCI controllers to schedule transfers.
 * It contains a horizontal link pointer, endpoint characteristics,
 * endpoint capabilities, a pointer to the current qTD, and an overlay area
 * that mimics the structure of a qTD. The overlay area is used by the hardware
 * to manage ongoing transfers.
 *
 * The total size of this structure is typically 64 bytes. Depending on your
 * implementation, you might not use all fields, but this definition provides a
 * complete layout per the EHCI specification.
 */
typedef struct ehci_qh {
    uint32_t horizontalLink;         // Pointer to the next QH (or termination flag)
    uint32_t endpointCharacteristics;  // Contains device address, endpoint number, etc.
    uint32_t endpointCapabilities;     // Maximum packet size, mult, etc.
    uint32_t currentQTD;               // Pointer to the currently active qTD

    // Overlay area that replicates the structure of a qTD
    uint32_t overlayNextTD;            // Next qTD pointer (overlay)
    uint32_t overlayToken;             // qTD token field (status, error bits, etc.)
    uint32_t overlayBufferPointers[5]; // Buffer pointers (up to 5 for scattered transfers)

    // Optional reserved fields to pad the structure to 64 bytes
    uint32_t reserved[4];              // Reserved/Padding
} ehci_qh;

/* 
 * USB Host Controller Type Enumeration
 *
 * This enum represents the types of USB host controllers supported.
 */
typedef enum {
    USB_HC_UHCI = 0,  // Universal Host Controller Interface (USB 1.1)
    USB_HC_OHCI = 1,  // Open Host Controller Interface (USB 1.1)
    USB_HC_EHCI = 2,  // Enhanced Host Controller Interface (USB 2.0)
    USB_HC_XHCI = 3   // eXtensible Host Controller Interface (USB 3.x)
} usb_hc_type_t;

typedef enum {
    USB_DEVICE_UNKNOWN,
    USB_DEVICE_KEYBOARD,
    // ... other device types as needed
} usb_device_type_t;

/*
 * USB Setup Packet Structure
 *
 * A USB setup packet is always 8 bytes long and used for control transfers.
 * The __attribute__((packed)) directive ensures there is no padding.
 */
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType; // Characteristics of request: direction, type, recipient
    uint8_t  bRequest;      // Specific request
    uint16_t wValue;        // Word-sized field that varies according to request
    uint16_t wIndex;        // Typically used to pass an index or offset
    uint16_t wLength;       // Number of bytes to transfer if there is a data stage
} usb_setup_packet_t;

/*
 * UHCI Queue Head (UHCI QH)
 *
 * A simplified structure representing a UHCI Queue Head.
 * This structure is used in the scheduling of transfer descriptors (TDs)
 * for control, bulk, or interrupt transfers.
 */
typedef struct uhci_qh {
    uint32_t head;    // Pointer to the next QH in the list (or termination flag)
    uint32_t element; // Pointer to the first Transfer Descriptor (TD) in this QH
} uhci_qh;

typedef struct {
    uint8_t address;             // USB address assigned during enumeration
    usb_device_type_t type;      // Type of device (e.g., keyboard)
    usb_hc_type_t controller;    // Which host controller (UHCI, EHCI, etc.) enumerated the device
    // Additional fields as needed (e.g., endpoint info, device descriptors, etc.)
} usb_device_t;

/*
 * USB Host Controller Structure
 *
 * Contains state and configuration for a USB host controller.
 * The union below has controller-specific members.
 */
typedef struct usb_host_controller {
    usb_hc_type_t type;         // UHCI, OHCI, EHCI, xHCI
    pci_device_t *pci_dev;      // Pointer to PCI device
    uint32_t io_base;           // I/O base address (for UHCI)
    volatile uint32_t *op_base; // Memory-mapped register base (for OHCI/EHCI/xHCI)
    uint8_t num_ports;          // Number of root hub ports
    union {
        struct {
            uint32_t *frame_list;   // 4KB-aligned frame list for scheduling transfers
            uhci_qh *control_qh;    // Control Queue Head
        } uhci;
        struct {
            uint32_t *periodic_list;   // 4KB-aligned periodic frame list
            ehci_qh *async_head;       // Asynchronous queue head for control transfers
        } ehci;
        // Structures for OHCI and xHCI can be added here.
    };
} usb_host_controller_t;

// Global USB device list and device count
extern usb_device_t usb_devices[MAX_USB_DEVICES];
extern int usb_device_count;

/* Make the common enumeration function public so that EHCI code can call it */
void usb_process_device_connect(usb_host_controller_t *hc, uint8_t port, uint8_t low_speed);

// Function prototypes for USB initialization and transfers
void usb_init(void);
int usb_read(usb_device_t *dev, void *buffer, int length);
void usb_poll(void);

void usb_print_devices(void);
void* usb_alloc_page(void);

#endif // USB_H
