#include <drivers/usb_keyboard.h>
#include <drivers/usb.h>
#include <printf.h>

// In a full implementation, you’d have a routine to look up a USB device by type.
// Here, we simulate by creating a static device.
static usb_device_t *keyboard_device = 0;

void usb_keyboard_init(void) {
    kprintf("Initializing USB keyboard driver...\n");
    
    // In a complete driver, search the enumerated USB devices for one matching a keyboard.
    // For this demo, we assume the first device (address 1) is the keyboard.
    static usb_device_t dummy_keyboard = { .address = 1, .type = USB_DEVICE_KEYBOARD };
    keyboard_device = &dummy_keyboard;
    
    // Additional endpoint and configuration initialization would go here.
}

void usb_keyboard_poll(void) {
    if (!keyboard_device)
        return;
    
    uint8_t report[8];
    int bytes_read = usb_read(keyboard_device, report, 8);
    if (bytes_read > 0) {
        // For this simple example, assume report[2] holds a key usage code.
        uint8_t key_code = report[2];
        if (key_code) {
            // Convert the HID usage code to an ASCII character.
            // In the HID standard, 0x04 maps to 'a', 0x05 to 'b', etc.
            char c = 'a' + (key_code - 0x04);
            kprintf("USB Keyboard: Key '%c' pressed.\n", c);
        }
    }
}
