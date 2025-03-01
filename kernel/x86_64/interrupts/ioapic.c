#include <interrupts/ioapic.h>
#include <acpi/tables.h>
#include <acpi/acpi.h>
#include <printf.h>
#include <panic.h>
#include <io.h>

// These global pointers are used to access the I/O APIC registers.
static volatile uint32_t *IOREGSEL = NULL;
static volatile uint32_t *IOREGWIN = NULL;

#define IOREDTBL    0x10   // I/O Redirection Table register offset base

// Parse the MADT to locate and map the I/O APIC registers.
bool ioapic_init(void) {
    const acpi_madt_t *madt = (const acpi_madt_t *) acpi_find_table("APIC", 0);
    if (!madt || !acpi_checksum((void*)madt)) {
        kprintf("ERROR: MADT (APIC table) not found or invalid.\n");
        return false;
    }

    uint32_t madt_length = madt->hdr.len;
    uint8_t *madt_end = (uint8_t *)madt + madt_length;
    uint8_t *entry = (uint8_t *)madt + sizeof(acpi_madt_t);
    IOREGSEL = IOREGWIN = NULL;
    kprintf("ACPI MADT: LAPIC addr=0x%x, Flags=0x%x\n", madt->local_interrupt_controller_addr, madt->flags);
    
    while (entry < madt_end) {
        acpi_madt_entry_t *header = (acpi_madt_entry_t *)entry;
        switch(header->type) {
            case 0x01: { // I/O APIC entry
                apic_io_t *ioapic = (apic_io_t *)entry;
                kprintf(" MADT: I/O APIC ID=%u, Addr=0x%x, GSI Base=%u\n",
                        ioapic->id, ioapic->address, ioapic->globalSystemInterruptBase);
                // Select the I/O APIC with GSI base 0 (for primary ISA IRQs)
                if (IOREGSEL == NULL && ioapic->globalSystemInterruptBase == 0) {
                    IOREGSEL = (volatile uint32_t *)(uintptr_t) ioapic->address;
                    IOREGWIN = (volatile uint32_t *)(uintptr_t)(ioapic->address + 0x10);
                }
                break;
            }
            // You can add additional cases for other MADT entry types if needed.
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
void ioapic_map_irq(uint8_t irq, uint8_t vector) {
    if (!IOREGSEL || !IOREGWIN) {
        kprintf("ioapic_map_irq: I/O APIC not initialized\n");
        return;
    }
    kprintf("IOAPIC: Mapping IRQ %hhu -> IDT vector %hhu\n", irq, vector);
    uint32_t index = IOREDTBL + irq * 2;
    uint32_t hi_index = index + 1;
    
    // Read current APIC ID (assumes a single-processor environment).
    uint32_t apic_id = 0;
    // (Assuming a function or macro 'lapic_read' is available if needed; alternatively, you can
    // provide a method to obtain the local APIC ID.)
    
    // Write the high register to set the destination (APIC ID).
    *(IOREGSEL) = hi_index;
    uint32_t hi_val = *(IOREGWIN);
    hi_val &= 0x00FFFFFF;  // Clear the destination field.
    hi_val |= ((uint32_t)apic_id) << 24;
    *(IOREGWIN) = hi_val;
    
    // Write the low register: set the vector, unmask (bit 16 = 0), fixed delivery mode.
    *(IOREGSEL) = index;
    uint32_t lo_val = *(IOREGWIN);
    lo_val &= ~(1 << 16);         // Ensure the mask bit is cleared.
    lo_val &= ~(1 << 11);         // Ensure physical destination mode.
    lo_val &= ~(0x7 << 8);        // Clear delivery mode bits.
    lo_val &= ~0xFF;              // Clear any old vector.
    lo_val |= vector;             // Set the new vector.
    *(IOREGWIN) = lo_val;
}
