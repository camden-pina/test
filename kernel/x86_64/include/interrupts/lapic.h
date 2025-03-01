#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>
#include <stdbool.h>

// Initialize Local APIC and disable legacy PIC.
void lapic_init(void);

// Send End-of-Interrupt signal if needed.
void apic_send_eoi_if_necessary(uint8_t vector);

#endif // LAPIC_H
