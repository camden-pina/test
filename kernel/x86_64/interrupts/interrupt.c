// Interrupt handlers and common ISR logic (interrupt.c)
#include <printf.h>
#include <panic.h>
#include <stdint.h>
#include <stddef.h>
#include <interrupts/lapic.h>

// Array of function pointers for custom IRQ handlers (indexed by vector number)
static void (*interrupts_handlers[256])(int error_code);

// Default stub handler: used for unhandled IRQs 
static void default_interrupt_handler(int irq_num) {
    kprintf("Unhandled interrupt: vector %d (no handler registered)\n", irq_num);
    // Just acknowledge and return; system continues running
}

// Exception messages table (0-31)
static const char *exception_names[32] = {
    "#DE Divide Error",
    "#DB Debug",
    "Non-Maskable Interrupt",
    "#BP Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available (No Math Coprocessor)",
    "Double Fault",
    "Coprocessor Segment Overrun (reserved)",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

void interrupts_init_handlers(void) {
    // Initialize all entries with default handler
    for (int i = 0; i < 256; ++i) {
        interrupts_handlers[i] = default_interrupt_handler;
    }
}

// Register a handler for a given IRQ/interrupt vector.
void register_interrupt_handler(uint8_t vector, void (*handler)(int)) {
    if (vector < 256 && handler != NULL) {
        interrupts_handlers[vector] = handler;
    } else {
        kprintf("register_interrupt_handler: invalid vector %u or handler\n", vector);
    }
}

// Common ISR handler called from assembly stubs.
// Parameters: interrupt number and error code (error_code is meaningful for some exceptions; for hardware IRQs it may be 0).
void isr_common(uint8_t vector, int error_code) {
    // Identify whether this is an exception or an external interrupt
    if (vector < 32) {
        // CPU exception
        const char *excName = exception_names[vector];
        kprintf("\n[EXCEPTION] CPU Exception %u: %s\n", vector, excName ? excName : "Unknown");
        if (vector == 14) {
            // Example: for Page Fault (vector 14), print CR2 or related info if accessible
            uintptr_t fault_addr;
            __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
            kprintf("CR2 (Faulting address) = 0x%p\n", (void*)fault_addr);
        }
        if (error_code != 0) {
            kprintf("Error Code: 0x%x\n", error_code);
        }
        kprintf("System HALTED due to above exception.\n");
        __asm__ volatile("cli; hlt");  // disable interrupts and halt indefinitely
    } else {
        // Hardware IRQ or software interrupt
        void (*handler)(int) = interrupts_handlers[vector];
        if (handler) {
            handler(error_code);  // call the installed handler with the error code (typically 0 for IRQ)
        } else {
            // Should not happen since we set a default handler for all, but just in case:
            kprintf("Warning: No handler for IRQ %u\n", vector);
        }
        // Send End-of-Interrupt signal for hardware interrupts if needed
        apic_send_eoi_if_necessary(vector);
    }
}
