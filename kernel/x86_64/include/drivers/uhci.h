#ifndef UHCI_H
#define UHCI_H

#include <stdint.h>
#include "usb.h"      // Must define usb_host_controller_t, usb_setup_packet_t, etc.
#include "uhci_td.h"  // UHCI TD definitions

/* Prototype for the UHCI control transfer function */
int uhci_control_transfer(usb_host_controller_t *hc, uint8_t device_address,
                          usb_setup_packet_t *setup, void *buffer, int length);

/* Function to register the UHCI interrupt (maps IRQ to vector) */
void uhci_register_interrupt(pci_device_t *pci_dev);

/* UHCI-specific interrupt handler main (called from the assembly wrapper) */
void uhci_interrupt_handler_main(uint64_t vector, uint32_t error);

/* Global array to hold UHCI controllers for interrupt processing */
#define MAX_UHCI_CONTROLLERS 8
extern usb_host_controller_t uhci_controllers[MAX_UHCI_CONTROLLERS];
extern int num_uhci_controllers;

#endif // UHCI_H
