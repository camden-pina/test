#ifndef KERNEL_WM_H
#define KERNEL_WM_H

#include <stdint.h>

void wm_handle_keyboard_scancode(uint8_t scancode);
void wm_handle_mouse(int8_t dx, int8_t dy, uint8_t status);

void wm_init(void);
void wm_run(void);

#endif
