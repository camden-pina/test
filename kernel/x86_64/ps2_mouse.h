#ifndef _PS2_MOUSE_H
#define _PS2_MOUSE_H 1

#include <descriptor_tables/isr.h>
#include <interrupts/ioapic.h>

#include <io.h>

void ps2_mouse_init(void);
unsigned long long get_mouse_x(void);
unsigned long long get_mouse_y(void);

#endif
