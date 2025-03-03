#ifndef USB_H
#define USB_H

#include <stdint.h>
#include <drivers/pci.h>

#define MAX_USB_DEVICES 16

/*
 * USB Host Controller Type Enumeration
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
 * USB Setup Packet (8 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

/*
 * EHCI Queue Head (EHCI QH)
 */
typedef struct ehci_qh {
    uint32_t horizontalLink;
    uint32_t endpointCharacteristics;
    uint32_t endpointCapabilities;
    uint32_t currentQTD;

    // Overlay area
    uint32_t overlayNextTD;
    uint32_t overlayToken;
    uint32_t overlayBufferPointers[5];
    uint32_t reserved[4];
} ehci_qh;

/*
 * UHCI Queue Head (UHCI QH)
 */
typedef struct uhci_qh {
    uint32_t head;
    uint32_t element;
} uhci_qh;

/*
 * xHCI Data Structures (simplified for demonstration)
 *
 * We embed xHCI-specific state in a sub-structure for clarity.
 */
#define XHCI_MAX_SLOTS          256
#define XHCI_CMD_RING_SIZE      256
#define XHCI_EVENT_RING_SIZE    256

typedef struct xhci_trb {
    uint32_t field[4];
} xhci_trb_t;

/*
 * xHCI-specific host controller state
 */
typedef struct xhci_state {
    volatile uint32_t *db_regs;   // Doorbell registers base
    volatile uint32_t *run_regs;  // Runtime registers base
    uint64_t          *dcbaa;     // Device Context Base Address Array
    void              *device_context[XHCI_MAX_SLOTS];

    // Command ring
    xhci_trb_t *cmd_ring;
    uint16_t     cmd_ring_index;
    uint8_t      cmd_cycle;

    // Event ring
    xhci_trb_t *event_ring;
    uint16_t     evt_ring_index;
    uint8_t      evt_cycle;

    // Mapping of USB address to xHCI slot ID
    uint8_t slot_for_address[256];

    // The maximum number of slots the controller supports
    uint8_t  max_slots;
} xhci_state_t;

/*
 * USB Device Information
 */
typedef struct {
    uint8_t address;             // USB address assigned during enumeration
    usb_device_type_t type;      // Type of device (e.g., keyboard)
    usb_hc_type_t controller;    // Which host controller enumerated this device
    // Additional fields as needed...
} usb_device_t;

/*
 * USB Host Controller
 */
typedef struct usb_host_controller {
    usb_hc_type_t type;         // UHCI, OHCI, EHCI, xHCI
    pci_device_t *pci_dev;      // The associated PCI device
    uint32_t      io_base;      // I/O base (for UHCI/OHCI/EHCI)
    volatile uint32_t *op_base; // Memory mapped base (for EHCI/xHCI/others)
    uint8_t       num_ports;    // Number of root hub ports discovered

    union {
        struct {
            // UHCI structures
            uint32_t *frame_list;
            uhci_qh  *control_qh;
        } uhci;
        struct {
            // EHCI structures
            uint32_t *periodic_list;
            ehci_qh  *async_head;
        } ehci;
        struct {
            // xHCI-specific
            xhci_state_t xhci;
        } x;
    };
} usb_host_controller_t;

/*
 * Global arrays for devices and host controllers.
 * These are defined in usb.c, but we declare them as extern here so other files can use them.
 */
extern usb_device_t usb_devices[MAX_USB_DEVICES];
extern int          usb_device_count;

extern usb_host_controller_t usb_controllers[8];
extern int                  usb_hc_count;

/*
 * Common USB functions
 */
void  usb_init(void);
void  usb_poll(void);
int   usb_read(usb_device_t *dev, void *buffer, int length);
void  usb_print_devices(void);
void* usb_alloc_page(void);

void  usb_process_device_connect(usb_host_controller_t *hc, uint8_t port, uint8_t low_speed);

/*
 * Common Control Transfer function
 */
int   usb_control_transfer(usb_host_controller_t *hc, uint8_t device_address,
                           usb_setup_packet_t *setup, void *buffer, int length);

/*
 * Additional helper for device type names, etc.
 */
const char* usb_device_type_name(usb_device_type_t type);
const char* usb_host_controller_name(usb_hc_type_t type);

/*
 * xHCI-specific initialization and transfer routines
 * (defined in xhci.c)
 */
int init_xhci_controller(pci_device_t *pci_dev);
int xhci_control_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                          usb_setup_packet_t *setup, void *buffer, int length);

/* We also define xhci_bulk_transfer(), xhci_interrupt_transfer(), etc. if needed */

#endif // USB_H
