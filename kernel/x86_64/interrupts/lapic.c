#include <interrupts/lapic.h>
#include <acpi/acpi.h>
#include <msr.h>
#include <io.h>
#include <printf.h>
#include <panic.h>
#include <acpi/tables.h>  // Needed for MADT parsing when checking for overrides

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

static uint32_t *local_apic_base = NULL;  // Mapped address for Local APIC registers

// Low-level functions to read and write Local APIC registers.
static inline void lapic_write(uint32_t reg_offset, uint32_t value) {
    *(volatile uint32_t *)((uintptr_t)local_apic_base + reg_offset) = value;
}

static inline uint32_t lapic_read(uint32_t reg_offset) {
    return *(volatile uint32_t *)((uintptr_t)local_apic_base + reg_offset);
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

// Public function to initialize the Local APIC.
void lapic_init(void) {
    kprintf("Initializing Local APIC...\n");

    // Disable the legacy PIC.
    disable_pic();

    // Read the APIC Base MSR.
    uint64_t apic_base_msr = read_msr(MSR_IA32_APIC_BASE);
    local_apic_base = (uint32_t *)(uintptr_t)(apic_base_msr & 0xFFFFF000ULL);
    
    // Enable APIC globally.
    apic_base_msr |= (1ULL << 11);
    write_msr(MSR_IA32_APIC_BASE, apic_base_msr);
    kprintf("Local APIC base=%p (from MSR), APIC enabled via MSR (0x%llx)\n", 
            local_apic_base, apic_base_msr);

    // Set the Spurious Interrupt Vector Register (SVR).
    uint32_t svr = lapic_read(LAPIC_SVR);
    svr &= 0xFFFFFF00;   // Clear the vector portion.
    svr |= 0xFF;         // Set spurious vector to 0xFF.
    svr |= (1 << 8);     // Enable APIC in software.
    lapic_write(LAPIC_SVR, svr);
    kprintf("LAPIC: SVR set to 0x%08x (spurious vector=0x%02X, APIC enabled)\n", svr, svr & 0xFF);

    // Mask legacy LINT0 and LINT1.
    uint32_t lint0 = lapic_read(LAPIC_LVT_LINT0);
    uint32_t lint1 = lapic_read(LAPIC_LVT_LINT1);
    lint0 |= (1 << 16);
    lint1 |= (1 << 16);
    lapic_write(LAPIC_LVT_LINT0, lint0);
    lapic_write(LAPIC_LVT_LINT1, lint1);
    kprintf("LAPIC: LINT0/LINT1 masked (0x%08x, 0x%08x)\n", lint0, lint1);

    kprintf("Local APIC initialization complete.\n");
}

// Public function to send an End-of-Interrupt signal.
void apic_send_eoi_if_necessary(uint8_t vector) {
    // Check if the interrupt vector is in-service.
    uint32_t isr_reg_index = 0x100 + (vector / 32) * 0x10;
    uint32_t isr_bit = 1u << (vector % 32);
    uint32_t isr_val = lapic_read(isr_reg_index);
    if (isr_val & isr_bit) {
        lapic_write(LAPIC_EOI, 0);
    }
}
