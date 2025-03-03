#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <drivers/usb.h>  // usb_host_controller_t, usb_setup_packet_t

/*
 * We place additional xHCI constants and prototypes here.
 * The actual structure definitions are in usb.h under xhci_state_t.
 *
 * If you prefer a separate structure not embedded in usb_host_controller_t,
 * you can do that as well. For now, we keep it in the union { } x.xhci.
 */

/* TRB types */
#define TRB_TYPE_SETUP_STAGE         2
#define TRB_TYPE_DATA_STAGE          3
#define TRB_TYPE_STATUS_STAGE        4
#define TRB_TYPE_CMD_COMPLETION      33
#define TRB_TYPE_TRANSFER_EVENT      32
#define TRB_TYPE_PORT_STATUS_CHANGE  34
#define TRB_TYPE_ENABLE_SLOT         9
#define TRB_TYPE_ADDRESS_DEVICE      11

#define XHCI_COMP_SUCCESS           1
#define XHCI_COMP_SHORT_PACKET     13

/* Function prototypes for the xHCI driver. */
int init_xhci_controller(pci_device_t *pci_dev);

int xhci_control_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                          usb_setup_packet_t *setup, void *buffer, int length);

int xhci_bulk_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                       uint8_t endpoint, void *buffer, int length);

int xhci_interrupt_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                            uint8_t endpoint, void *buffer, int length);

int xhci_iso_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                      uint8_t endpoint, void *buffer, int length, uint32_t frame);

#endif // XHCI_H
