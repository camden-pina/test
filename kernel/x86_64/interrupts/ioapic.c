#include <interrupts/ioapic.h>
#include <interrupts/lapic.h>
#include <acpi/tables.h>
#include <acpi/acpi.h>
#include <printf.h>
#include <panic.h>
#include <io.h>
#include <cpu.h>
#include <interrupts/lapic.h>

// Global pointers for accessing the I/O APIC registers.
static volatile uint32_t *IOREGSEL = NULL;
static volatile uint32_t *IOREGWIN = NULL;

#define IOREDTBL    0x10   // I/O Redirection Table register offset base

uint16_t enabled_apic_count = 0;
uint16_t online_capable_apic_count = 0;
uint16_t total_apic_count = 0;
uint8_t apic_id_map[MAX_CPUS];

// Parse the MADT to locate and map the I/O APIC registers.
bool ioapic_init(void) {
    const acpi_madt_t *madt = (const acpi_madt_t *) acpi_find_table("APIC", 0);
    if (!madt) {
        kprintf("ERROR: MADT (APIC table) not found.\n");
        return false;
    }
    if (!acpi_checksum((void*)madt)) {
        kprintf("ERROR: MADT (APIC table) checksum invalid.\n");
        return false;
    }

    uint32_t madt_length = madt->hdr.len;
    uint8_t *madt_end = (uint8_t *)madt + madt_length;
    uint8_t *entry = (uint8_t *)madt + sizeof(acpi_madt_t);
    IOREGSEL = IOREGWIN = NULL;
    kprintf("ACPI MADT: LAPIC addr=0x%x, Flags=0x%x\n", madt->local_interrupt_controller_addr, madt->flags);
    
    while (entry < madt_end) {
        acpi_madt_entry_t *header = (acpi_madt_entry_t *)entry;
        if (header->length == 0) {
            kprintf("ERROR: Detected MADT entry with zero length, aborting parsing.\n");
            return false;
        }
        switch(header->type) {
            case 0x00: { // Processor Local APIC
                apic_local_t *local_apic = (void *)entry;
                if ((local_apic->flags & ACPI_MADT_APIC_FLAG_ENABLED) != 0) {
                    enabled_apic_count++;
                } else if ((local_apic->flags & ACPI_MADT_APIC_FLAG_ONLINE_CAP) != 0) {
                    online_capable_apic_count++;
                }
                kassert(total_apic_count < MAX_CPUS);
                apic_id_map[total_apic_count++] = local_apic->apicID;
                register_apic(local_apic->apicID);
                break;
            }
            case 0x01: { // I/O APIC entry
                apic_io_t *ioapic = (apic_io_t *)entry;
                kprintf(" MADT: I/O APIC ID=%u, Addr=0x%x, GSI Base=%u\n",
                        ioapic->id, ioapic->address, ioapic->globalSystemInterruptBase);
                // Select the I/O APIC with GSI base 0 (for primary ISA IRQs).
                if (IOREGSEL == NULL && ioapic->globalSystemInterruptBase == 0) {
                    IOREGSEL = (volatile uint32_t *)(uintptr_t) ioapic->address;
                    IOREGWIN = (volatile uint32_t *)(uintptr_t)(ioapic->address + 0x10);
                    if (!IOREGSEL || !IOREGWIN) {
                        kprintf("ERROR: Failed to map I/O APIC registers for I/O APIC ID=%u\n", ioapic->id);
                        return false;
                    }
                }
                break;
            }
            // Add additional cases for other MADT entry types if needed.
            default:
                break;
        }
        entry += header->length;
    }
    
    if (IOREGSEL == NULL) {
        kprintf("ERROR: No I/O APIC found for GSI base 0\n");
        return false;
    }
    
    return true;
}

// Map an ISA IRQ or GSI to an IDT vector using the I/O APIC.
uint8_t ioapic_map_irq(uint8_t irq, uint8_t vector) {
    if (!IOREGSEL || !IOREGWIN) {
        kprintf("ERROR: ioapic_map_irq: I/O APIC not initialized\n");
        return 0;
    }

    // Validate IRQ and vector values.
    if (irq > 24) {  // Example limit; adjust based on your system.
        kprintf("ERROR: ioapic_map_irq: IRQ %u is out of range\n", irq);
        return 0;
    }
    if (vector < 32 || vector > 255) {  // Ensure vector is within the valid IDT range.
        kprintf("ERROR: ioapic_map_irq: vector %u is out of valid IDT range (32-255)\n", vector);
        return 0;
    }
    
    kprintf("IOAPIC: Mapping IRQ %u -> IDT vector %u\n", irq, vector);
    uint32_t index = IOREDTBL + irq * 2;
    uint32_t hi_index = index + 1;
    
    // Obtain the local APIC ID by reading the LAPIC_ID register.
    uint32_t apic_id = lapic_read(LAPIC_ID) >> 24;
    
    // Write to the high register to set the destination (APIC ID).
    *(IOREGSEL) = hi_index;
    uint32_t hi_val = *(IOREGWIN);
    hi_val &= 0x00FFFFFF;  // Clear the destination field.
    hi_val |= ((uint32_t)apic_id) << 24;
    *(IOREGWIN) = hi_val;
    
    // Write to the low register: set the vector, clear the mask bit, and specify fixed delivery mode.
    *(IOREGSEL) = index;
    uint32_t lo_val = *(IOREGWIN);
    lo_val &= ~(1 << 16);         // Clear the mask bit.
    lo_val &= ~(1 << 11);         // Clear physical destination mode bit.
    lo_val &= ~(0x7 << 8);        // Clear delivery mode bits.
    lo_val &= ~0xFF;              // Clear any previous vector.
    lo_val |= vector;             // Set the new vector.
    *(IOREGWIN) = lo_val;

    return vector;
}
