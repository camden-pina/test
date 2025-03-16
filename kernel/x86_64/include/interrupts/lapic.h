#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>
#include <stdbool.h>

// PIC ports and initialization control words.
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

#define ICW1_INIT   0x10  // Start initialization
#define ICW1_ICW4   0x01  // Expect ICW4
#define ICW4_8086   0x01  // 8086 mode

// Local APIC register offsets.
#define LAPIC_ID          0x020
#define LAPIC_VER         0x030
#define LAPIC_TPR         0x080
#define LAPIC_EOI         0x0B0
#define LAPIC_SVR         0x0F0
#define LAPIC_LVT_TIMER   0x320
#define LAPIC_LVT_LINT0   0x350
#define LAPIC_LVT_LINT1   0x360
#define LAPIC_LVT_ERROR   0x370

// Initialize Local APIC and disable legacy PIC.
void lapic_init(void);

// Send End-of-Interrupt signal if needed.
void apic_send_eoi_if_necessary(uint8_t vector);

// Add this prototype:
void lapic_set_tpr(uint8_t priority);

uint32_t lapic_read(uint32_t reg_offset);

void lapic_write(uint32_t reg_offset, uint32_t value);

#endif // LAPIC_H
