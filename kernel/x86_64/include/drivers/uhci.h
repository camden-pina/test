#ifndef UHCI_H
#define UHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <drivers/usb.h>     // usb_host_controller_t, usb_setup_packet_t, etc.
#include <drivers/uhci_td.h> // UHCI TD definitions

/**
 * Represents a single UHCI controller in your system.
 * Instead of embedding usb_host_controller_t, just point to one.
 */
typedef struct uhci_controller {
    usb_host_controller_t *hc;  // points to the generic USB controller struct
    uint8_t                irq; 
    bool                   irq_registered;
} uhci_controller_t;

/* Maximum # of UHCI controllers */
#define MAX_UHCI_CONTROLLERS 8

/* A global array of them (defined in uhci.c) */
extern uhci_controller_t uhci_controllers[MAX_UHCI_CONTROLLERS];
extern int               num_uhci_controllers;

/* Functions */
void uhci_register_interrupt(uhci_controller_t *controller);
void uhci_interrupt_handler_main(uint64_t vector, uint32_t error);

/* UHCI control transfer */
int uhci_control_transfer(usb_host_controller_t *hc, uint8_t device_address,
                          usb_setup_packet_t *setup, void *buffer, int length);

#endif // UHCI_H
