// interrupts.c
#include <printf.h>
#include <panic.h>
#include <stdint.h>
#include <stddef.h>
#include <interrupts/lapic.h>

// New structure that exactly reflects our desired exception frame.
typedef struct regs_full {
    /* General-purpose registers pushed by the stub (from lowest to highest address) */
    uint64_t rax;   /* Offset 0 */
    uint64_t rcx;   /* Offset 8 */
    uint64_t rdx;   /* Offset 16 */
    uint64_t rbx;   /* Offset 24 */
    uint64_t rbp;   /* Offset 32 */
    uint64_t rsi;   /* Offset 40 */
    uint64_t rdi;   /* Offset 48 */
    uint64_t r8;    /* Offset 56 */
    uint64_t r9;    /* Offset 64 */
    uint64_t r10;   /* Offset 72 */
    uint64_t r11;   /* Offset 80 */
    uint64_t r12;   /* Offset 88 */
    uint64_t r13;   /* Offset 96 */
    uint64_t r14;   /* Offset 104 */
    uint64_t r15;   /* Offset 112 */
    
         uint64_t vector;      // Offset 128
         uint64_t error_code;  // Offset 120
         uint64_t rip;         // Offset 136
         uint64_t cs;          // Offset 144
         uint64_t rflags;      // Offset 152
         // (Optionally, if applicable:)
         uint64_t rsp;         // Offset 160
         uint64_t ss;          // Offset 168
         uint64_t fxsave_state;
       
} regs_full_t;

// Exception names for vectors 0–31.
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

// For convenience, our default interrupt handler array (unchanged).
static void (*interrupts_handlers[256])(int error_code);

static void default_interrupt_handler(int irq_num) {
    kprintf("Unhandled interrupt: vector %d (no handler registered)\n", irq_num);
}

void interrupts_init_handlers(void) {
    for (int i = 0; i < 256; ++i) {
        interrupts_handlers[i] = default_interrupt_handler;
    }
}

void register_interrupt_handler(uint8_t vector, void (*handler)(int)) {
    if (vector < 256 && handler != NULL) {
        interrupts_handlers[vector] = handler;
    } else {
        kprintf("register_interrupt_handler: invalid vector %u or handler\n", vector);
    }
}

//
// New function: dump_exception_info
// This prints the header and all registers in exactly the format requested.
//
static void dump_exception_info(const regs_full_t *regs) {
    // Obtain segment registers (DS, ES, FS, GS)
    uint16_t ds, es, fs, gs;
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%es, %0" : "=r"(es));
    __asm__ volatile("mov %%fs, %0" : "=r"(fs));
    __asm__ volatile("mov %%gs, %0" : "=r"(gs));

    // Obtain control registers.
    uint64_t cr0, cr2, cr3, cr4, cr8;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("mov %%cr8, %0" : "=r"(cr8));

    // Obtain debug registers.
    uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
    __asm__ volatile("mov %%dr0, %0" : "=r"(dr0));
    __asm__ volatile("mov %%dr1, %0" : "=r"(dr1));
    __asm__ volatile("mov %%dr2, %0" : "=r"(dr2));
    __asm__ volatile("mov %%dr3, %0" : "=r"(dr3));
    __asm__ volatile("mov %%dr6, %0" : "=r"(dr6));
    __asm__ volatile("mov %%dr7, %0" : "=r"(dr7));

    // Obtain GDTR and IDTR.
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr, idtr;
    __asm__ volatile("sgdt %0" : "=m"(gdtr));
    __asm__ volatile("sidt %0" : "=m"(idtr));

    // Obtain LDTR and TR.
    uint16_t ldtr, tr;
    __asm__ volatile("sldt %0" : "=r"(ldtr));
    __asm__ volatile("str %0" : "=r"(tr));

    // Get the current CPU Apic ID.
    uint32_t apic_id = 0; // lapic_get_id();

    // For vector 13, print the short name "#GP - General Protection"
    const char *exc_name = (regs->vector == 13) ? "#GP - General Protection" : exception_names[regs->vector];

    // Print header with vector (in two-digit hex) and exception name.
    kprintf("\n\n!!!! X64 Exception Type - %02llX(%s)  CPU Apic ID - %08x !!!!\n",
            regs->vector, exc_name, apic_id);
    // Print ExceptionData (using the error code field)
    kprintf("ExceptionData - %016llx\n", regs->error_code);
    // Print RIP, CS, RFLAGS.
    kprintf("RIP  - %016llx, CS  - %016llx, RFLAGS - %016llx\n",
            regs->rip, regs->cs, regs->rflags);
    // Print general-purpose registers.
    kprintf("RAX  - %016llx, RCX - %016llx, RDX - %016llx\n",
            regs->rax, regs->rcx, regs->rdx);
    kprintf("RBX  - %016llx, RSP - %016llx, RBP - %016llx\n",
            regs->rbx, regs->rsp, regs->rbp);
    kprintf("RSI  - %016llx, RDI - %016llx\n",
            regs->rsi, regs->rdi);
    kprintf("R8   - %016llx, R9  - %016llx, R10 - %016llx\n",
            regs->r8, regs->r9, regs->r10);
    kprintf("R11  - %016llx, R12 - %016llx, R13 - %016llx\n",
            regs->r11, regs->r12, regs->r13);
    kprintf("R14  - %016llx, R15 - %016llx\n",
            regs->r14, regs->r15);
    // Print segment registers (DS, ES, FS, GS, SS).
    kprintf("DS   - %016llx, ES  - %016llx, FS  - %016llx\n",
            (uint64_t)ds, (uint64_t)es, (uint64_t)fs);
    kprintf("GS   - %016llx, SS  - %016llx\n",
            (uint64_t)gs, regs->ss);
    // Print control registers.
    kprintf("CR0  - %016llx, CR2 - %016llx, CR3 - %016llx\n",
            cr0, cr2, cr3);
    kprintf("CR4  - %016llx, CR8 - %016llx\n",
            cr4, cr8);
    // Print debug registers.
    kprintf("DR0  - %016llx, DR1 - %016llx, DR2 - %016llx\n",
            dr0, dr1, dr2);
    kprintf("DR3  - %016llx, DR6 - %016llx, DR7 - %016llx\n",
            dr3, dr6, dr7);
    // Print descriptor tables and registers.
    kprintf("GDTR(false-value) - %016llx %016llx, LDTR - %016llx\n",
            gdtr.base, (unsigned long long)gdtr.limit, (unsigned long long)ldtr);
    kprintf("IDTR - %016llx %016llx,   TR - %016llx\n",
            idtr.base, (unsigned long long)idtr.limit, (unsigned long long)tr);
    // Print FXSAVE_STATE.
    kprintf("FXSAVE_STATE(false-value) - %016llx\n", regs->fxsave_state);

}


// New isr_common that uses our new regs_full_t structure.
void isr_common(regs_full_t *regs) {
    // For exceptions (vectors 0–31) we print the full exception info.
    if (regs->vector == 3)
        dump_exception_info(regs);

    if (regs->vector < 32 && regs->vector != 3) {
        dump_exception_info(regs);
        kprintf("System HALTED due to above exception.\n");
        __asm__ volatile("cli; hlt");
    } else {
        // For hardware IRQs or software interrupts, call the registered handler.
        void (*handler)(int) = interrupts_handlers[regs->vector];
        if (handler) {
            handler((int)regs->error_code);
        } else {
            kprintf("Warning: No handler for IRQ %llu\n", regs->vector);
        }
        apic_send_eoi_if_necessary(regs->vector);
    }
}
