#ifndef USB_H
#define USB_H

#include <stdint.h>

// Basic USB device type.
typedef enum {
    USB_DEVICE_UNKNOWN,
    USB_DEVICE_KEYBOARD,
    // Extend this list with additional types as needed.
} usb_device_type_t;

// USB device structure.
typedef struct {
    uint8_t address;
    usb_device_type_t type;
    // Future fields: endpoint information, configuration data, etc.
} usb_device_t;

void usb_init(void);
void usb_poll(void);
int usb_read(usb_device_t *dev, void* buffer, int length);

#endif
