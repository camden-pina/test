#include <drivers/usb_keyboard.h>
#include <drivers/usb.h>
#include <printf.h>
#include <stdint.h>

static usb_device_t *keyboard_device = NULL;

void usb_keyboard_init(void) {
    kprintf("Initializing USB keyboard driver...\n");
    // Find the first enumerated USB keyboard device
    kprintf("Total USB Devices: %llu", usb_device_count);
    for (int i = 0; i < usb_device_count; ++i) {
        kprintf("[%llu]\n", usb_devices[i].type);
        if (usb_devices[i].type == USB_DEVICE_KEYBOARD) {
            keyboard_device = &usb_devices[i];
            break;
        }
    }
    if (keyboard_device) {
        kprintf("USB Keyboard: Device found at address %d\n", keyboard_device->address);
    } else {
        kprintf("USB Keyboard: No USB keyboard detected.\n");
    }
}

void usb_keyboard_poll(void) {
    if (!keyboard_device) return;
    uint8_t report[8];
    int read = usb_read(keyboard_device, report, sizeof(report));
    if (read > 0) {
        uint8_t key_code = report[2];  // HID usage code for the key
        if (key_code) {
            char ascii = 0;
            if (key_code >= 0x04 && key_code <= 0x1D) {
                // Letter keys: 0x04 -> 'a'
                ascii = 'a' + (key_code - 0x04);
            } else if (key_code == 0x2C) {
                ascii = ' ';  // space bar
            } else {
                // Other keys (e.g., enter, shift) could be handled here
                ascii = 0;
            }
            if (ascii) {
                kprintf("USB Keyboard: Key '%c' pressed.\n", ascii);
            } else {
                kprintf("USB Keyboard: Special key code 0x%02X pressed.\n", key_code);
            }
        }
    }
}
