#include <interrupts/lapic.h>
#include <acpi/acpi.h>
#include <msr.h>
#include <io.h>
#include <printf.h>
#include <panic.h>
#include <acpi/tables.h>  // Needed for MADT parsing when checking for overrides

static uint32_t *local_apic_base = NULL;  // Mapped address for Local APIC registers

// Low-level functions to read and write Local APIC registers.
inline void lapic_write(uint32_t reg_offset, uint32_t value) {
    if (!local_apic_base) {
        kprintf("ERROR: Attempt to write LAPIC register when local_apic_base is NULL\n");
        panic("lapic_write: local_apic_base is NULL");
    }
    *(volatile uint32_t *)((uintptr_t)local_apic_base + reg_offset) = value;
}

inline uint32_t lapic_read(uint32_t reg_offset) {
    if (!local_apic_base) {
        kprintf("ERROR: Attempt to read LAPIC register when local_apic_base is NULL\n");
        panic("lapic_read: local_apic_base is NULL");
    }
    return *(volatile uint32_t *)((uintptr_t)local_apic_base + reg_offset);
}

// Helper to set the LAPIC Task Priority Register (TPR).
void lapic_set_tpr(uint8_t priority) {
    lapic_write(LAPIC_TPR, (uint32_t)priority);
}

// Disable the legacy 8259 PIC by remapping and masking all interrupts.
static void disable_pic(void) {
    kprintf("Disabling legacy 8259 PIC...\n");

    // Start PIC initialization (cascade mode, expect ICW4)
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // Remap PIC vectors: Master to 0x20, Slave to 0x28.
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    io_wait();

    // Setup cascade configuration.
    outb(PIC1_DATA, 0x04);  // Master: there is a slave at IRQ2.
    outb(PIC2_DATA, 0x02);  // Slave: its cascade identity is 2.
    io_wait();

    // Set PICs to 8086 mode.
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Mask all IRQs.
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    kprintf("PIC remapped and masked (all IRQs off).\n");
}

void lapic_init(void) {
    kprintf("Initializing Local APIC...\n");
    disable_pic();

    // Read APIC base from MSR and enable the LAPIC in hardware.
    uint64_t apic_base_msr = read_msr(MSR_IA32_APIC_BASE);
    local_apic_base = (uint32_t *)(uintptr_t)(apic_base_msr & 0xFFFFF000ULL);
    if (!local_apic_base) {
        kprintf("ERROR: Failed to obtain a valid Local APIC base address from MSR\n");
        panic("lapic_init: local_apic_base is NULL");
    }
    apic_base_msr |= (1ULL << 11);  // Set the enable bit.
    write_msr(MSR_IA32_APIC_BASE, apic_base_msr);
    kprintf("Local APIC base=%p (from MSR), APIC enabled via MSR (0x%llx)\n", 
            local_apic_base, apic_base_msr);

    // Set the Spurious-Interrupt Vector Register (SVR).
    uint32_t svr = lapic_read(LAPIC_SVR);
    svr &= 0xFFFFFF00;   // Clear the vector portion.
    svr |= 0xFF;         // Set spurious vector = 0xFF.
    svr |= (1 << 8);     // Set APIC software-enable bit.
    lapic_write(LAPIC_SVR, svr);
    kprintf("LAPIC: SVR set to 0x%08x (spurious vector=0x%02X, APIC enabled)\n", svr, svr & 0xFF);

    // Mask LINT0 and LINT1.
    uint32_t lint0 = lapic_read(LAPIC_LVT_LINT0);
    uint32_t lint1 = lapic_read(LAPIC_LVT_LINT1);
    lint0 |= (1 << 16);
    lint1 |= (1 << 16);
    lapic_write(LAPIC_LVT_LINT0, lint0);
    lapic_write(LAPIC_LVT_LINT1, lint1);
    kprintf("LAPIC: LINT0/LINT1 masked (0x%08x, 0x%08x)\n", lint0, lint1);

    // Set TPR to 0 => unmask all interrupt priorities.
    lapic_set_tpr(0);
    kprintf("LAPIC: TPR set to 0 (no priority masking)\n");

    kprintf("Local APIC initialization complete.\n");
}

// Public function to send an End-of-Interrupt (EOI) signal if needed.
void apic_send_eoi_if_necessary(uint8_t vector) {
    if (!local_apic_base) {
        kprintf("ERROR: Cannot send EOI - local_apic_base is NULL\n");
        return;
    }
    // Calculate the In-Service Register (ISR) index and bit.
    uint32_t isr_reg_index = 0x100 + (vector / 32) * 0x10;
    uint32_t isr_bit = 1u << (vector % 32);
    kprintf("check1\n");
    uint32_t isr_val = lapic_read(isr_reg_index);
    kprintf("check2\n");
    if (isr_val & isr_bit) {
    kprintf("check3\n");
        lapic_write(LAPIC_EOI, 0);
    }
    kprintf("check4\n");
}
