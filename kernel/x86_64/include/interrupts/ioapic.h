#ifndef IOAPIC_H
#define IOAPIC_H

#include <stdint.h>
#include <stdbool.h>

// Initialize and map the I/O APIC registers by parsing the ACPI MADT.
// Returns true if successful, false otherwise.
bool ioapic_init(void);

// Map an ISA IRQ or GSI to an IDT vector.
void ioapic_map_irq(uint8_t irq, uint8_t vector);

#endif // IOAPIC_H
