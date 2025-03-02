#ifndef _IDT_H
#define _IDT_H 1

#include <interrupts/ioapic.h>
#include <descriptor_tables/isr.h>
#include "../kernel.h"

// Common IDT flag bits
#define IDT_FLAG_PRESENT         0x80
#define IDT_FLAG_RING0           0x00  // DPL = 0
#define IDT_FLAG_RING3           0x60  // DPL = 3

// Gate types (low 4 bits)
#define IDT_FLAG_INTERRUPT_GATE  0x0E
#define IDT_FLAG_TRAP_GATE       0x0F

// Idt functions
void idt_set_gate(uint8_t vector, uint64_t base, uint16_t sel, uint8_t flags);

void idt_init(void);

#endif
