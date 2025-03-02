// IDT and ISR setup (in idt.c)
#include <descriptor_tables/idt.h>
#include <descriptor_tables/gdt.h>
#include <interrupts/ioapic.h>
#include <interrupts/lapic.h>
#include <printf.h>
#include <panic.h>
#include <string.h>

/*
 * idt.c - Interrupt Descriptor Table (IDT) Initialization for x86_64
 *
 * This file defines the gate descriptor type for IDT entries,
 * sets up the IDT with entries for CPU exceptions, hardware IRQs,
 * and system calls, and loads the IDT using the lidt instruction.
 *
 * Best practices applied:
 *  - Clear separation of concerns with helper functions.
 *  - Robust and modular structure with detailed comments.
 *  - Consistent naming conventions and use of constants.
 *  - Error checking and self-documenting code.
 */

 // -----------------------------------------------------------------------------
 // Gate Descriptor Definition for x86_64 IDT
 // -----------------------------------------------------------------------------
 
 /*
  * gate_descriptor_t represents one entry in the IDT.
  * It is defined as follows for 64-bit mode:
  *
  *  Bits  0-15:  Lower 16 bits of the ISR address.
  *  Bits 16-31:  Code segment selector in the GDT.
  *  Bits 32-39:  IST (Interrupt Stack Table) offset (only bits 0-2 used; bits 3-7 are zero).
  *  Bits 40-47:  Type and attributes:
  *                  - Bit 7: Present flag.
  *                  - Bits 6-5: Descriptor Privilege Level (DPL).
  *                  - Bit 4: Zero.
  *                  - Bits 3-0: Gate type (0xE for interrupt gate, 0xF for trap gate).
  *  Bits 48-63:  Middle 16 bits of the ISR address.
  *  Bits 64-95:  Upper 32 bits of the ISR address.
  *  Bits 96-127: Reserved (must be zero).
  */
 typedef struct __attribute__((packed)) {
	 uint16_t offset_low;    // Lower 16 bits of ISR address.
	 uint16_t selector;      // Code segment selector in GDT.
	 uint8_t  ist;           // Bits 0-2: Interrupt Stack Table offset; bits 3-7: zero.
	 uint8_t  type_attr;     // Type and attributes (present, DPL, gate type).
	 uint16_t offset_mid;    // Middle 16 bits of ISR address.
	 uint32_t offset_high;   // Upper 32 bits of ISR address.
	 uint32_t zero;          // Reserved, must be zero.
 } gate_descriptor_t;
 
 // Total number of IDT entries.
 #define IDT_ENTRIES 256
 
 // -----------------------------------------------------------------------------
 // Global IDT and IDTR Structures
 // -----------------------------------------------------------------------------
 
 // Declare the IDT array, aligned on a 16-byte boundary.
 static gate_descriptor_t idt[IDT_ENTRIES] __attribute__((aligned(16)));
 
 /*
  * idtr_t represents the IDT Register (IDTR) structure used by the lidt instruction.
  * It contains:
  *  - limit: The size (in bytes) of the IDT minus one.
  *  - base:  The linear address of the first element in the IDT.
  */
 typedef struct __attribute__((packed)) {
	 uint16_t limit;
	 uint64_t base;
 } idtr_t;
 
 // -----------------------------------------------------------------------------
 // Helper Function: Set an IDT Entry
 // -----------------------------------------------------------------------------
 
 /*
  * idt_set_entry sets a single IDT entry.
  *
  * Parameters:
  *   vector   - The interrupt vector number (0 to 255).
  *   isr_addr - The address of the interrupt service routine.
  *   selector - The code segment selector (typically KERNEL_CS).
  *   ist      - The IST index (0 if not used, or 1-7 for specific critical exceptions).
  *   type     - The gate type: 0xE for interrupt gate, 0xF for trap gate.
  *   dpl      - Descriptor Privilege Level (0 for kernel, 3 for user-accessible interrupts).
  *   present  - Present flag (1 if this entry is valid).
  */
 static void idt_set_entry(uint8_t vector, uint64_t isr_addr, uint16_t selector,
						   uint8_t ist, uint8_t type, uint8_t dpl, uint8_t present) {
	 gate_descriptor_t *entry = &idt[vector];
	 entry->offset_low  = (uint16_t)(isr_addr & 0xFFFF);
	 entry->selector    = selector;
	 entry->ist         = ist & 0x07;  // Only lower 3 bits are valid.
	 entry->type_attr   = ((present & 0x01) << 7) | ((dpl & 0x03) << 5) | (type & 0x0F);
	 entry->offset_mid  = (uint16_t)((isr_addr >> 16) & 0xFFFF);
	 entry->offset_high = (uint32_t)((isr_addr >> 32) & 0xFFFFFFFF);
	 entry->zero        = 0;
 }
 
 // -----------------------------------------------------------------------------
 // External ISR Stubs
 // -----------------------------------------------------------------------------
 
 /*
  * These external declarations refer to the assembly stubs for each ISR.
  * You should have corresponding symbols (isr0, isr1, ..., isr47, isr128, etc.)
  * defined in your interrupt helper assembly file.
  */
 extern void isr0();
 extern void isr1();
 extern void isr2();
 extern void isr3();
 extern void isr4();
 extern void isr5();
 extern void isr6();
 extern void isr7();
 extern void isr8();
 extern void isr9();
 extern void isr10();
 extern void isr11();
 extern void isr12();
 extern void isr13();
 extern void isr14();
 extern void isr15();
 extern void isr16();
 extern void isr17();
 extern void isr18();
 extern void isr19();
 extern void isr20();
 extern void isr21();
 extern void isr22();
 extern void isr23();
 extern void isr24();
 extern void isr25();
 extern void isr26();
 extern void isr27();
 extern void isr28();
 extern void isr29();
 extern void isr30();
 extern void isr31();
 
 // Hardware IRQs: vectors 32-47.
 extern void isr32();
 extern void isr33();
 extern void isr34();
 extern void isr35();
 extern void isr36();
 extern void isr37();
 extern void isr38();
 extern void isr39();
 extern void isr40();
 extern void isr41();
 extern void isr42();
 extern void isr43();
 extern void isr44();
 extern void isr45();
 extern void isr46();
 extern void isr47();
 
 // System call (software interrupt) handler: vector 128.
 extern void isr128();
 
 // Optional: Spurious interrupt handler (e.g., vector 255).
 extern void isr_spurious();

 extern void uhci_isr80();
 
 // -----------------------------------------------------------------------------
 // IDT Initialization
 // -----------------------------------------------------------------------------
 
 /*
  * idt_init initializes the IDT by:
  *  - Clearing the IDT array.
  *  - Setting up entries for CPU exceptions (vectors 0–31).
  *  - Setting up entries for hardware IRQs (vectors 32–47).
  *  - Setting up the system call interrupt (vector 128) as a trap gate with DPL=3.
  *  - Optionally setting an entry for spurious interrupts (vector 255).
  *  - Loading the IDT using the lidt instruction.
  */
 void idt_init(void) {
	 // Clear the entire IDT.
	 memset(idt, 0, sizeof(idt));
 
	 // -------------------------------
	 // CPU Exceptions (Vectors 0–31)
	 // -------------------------------
	 // Use type 0xE (interrupt gate) for most exceptions and type 0xF (trap gate) for exceptions
	 // that need to be accessible from user space (e.g., breakpoint).
	 idt_set_entry(0,  (uint64_t)isr0,  KERNEL_CS, 0, 0xE, 0, 1);  // Divide Error (#DE)
	 idt_set_entry(1,  (uint64_t)isr1,  KERNEL_CS, 0, 0xE, 0, 1);  // Debug (#DB)
	 idt_set_entry(2,  (uint64_t)isr2,  KERNEL_CS, 1, 0xE, 0, 1);  // Non-Maskable Interrupt (NMI) uses IST 1
	 idt_set_entry(3,  (uint64_t)isr3,  KERNEL_CS, 0, 0xF, 3, 1);  // Breakpoint (INT3) – trap gate, accessible from user mode
	 idt_set_entry(4,  (uint64_t)isr4,  KERNEL_CS, 0, 0xE, 0, 1);  // Overflow
	 idt_set_entry(5,  (uint64_t)isr5,  KERNEL_CS, 0, 0xE, 0, 1);  // Bound Range Exceeded
	 idt_set_entry(6,  (uint64_t)isr6,  KERNEL_CS, 0, 0xE, 0, 1);  // Invalid Opcode
	 idt_set_entry(7,  (uint64_t)isr7,  KERNEL_CS, 0, 0xE, 0, 1);  // Device Not Available
	 idt_set_entry(8,  (uint64_t)isr8,  KERNEL_CS, 2, 0xE, 0, 1);  // Double Fault – uses IST 2
	 idt_set_entry(9,  (uint64_t)isr9,  KERNEL_CS, 0, 0xE, 0, 1);  // Coprocessor Segment Overrun (reserved)
	 idt_set_entry(10, (uint64_t)isr10, KERNEL_CS, 0, 0xE, 0, 1);  // Invalid TSS
	 idt_set_entry(11, (uint64_t)isr11, KERNEL_CS, 0, 0xE, 0, 1);  // Segment Not Present
	 idt_set_entry(12, (uint64_t)isr12, KERNEL_CS, 0, 0xE, 0, 1);  // Stack-Segment Fault
	 idt_set_entry(13, (uint64_t)isr13, KERNEL_CS, 0, 0xE, 0, 1);  // General Protection Fault
	 idt_set_entry(14, (uint64_t)isr14, KERNEL_CS, 0, 0xE, 0, 1);  // Page Fault
	 idt_set_entry(15, (uint64_t)isr15, KERNEL_CS, 0, 0xE, 0, 1);  // Reserved
	 idt_set_entry(16, (uint64_t)isr16, KERNEL_CS, 0, 0xE, 0, 1);  // x87 Floating-Point Exception
	 idt_set_entry(17, (uint64_t)isr17, KERNEL_CS, 0, 0xE, 0, 1);  // Alignment Check
	 idt_set_entry(18, (uint64_t)isr18, KERNEL_CS, 0, 0xE, 0, 1);  // Machine Check
	 idt_set_entry(19, (uint64_t)isr19, KERNEL_CS, 0, 0xE, 0, 1);  // SIMD Floating-Point Exception
	 idt_set_entry(20, (uint64_t)isr20, KERNEL_CS, 0, 0xE, 0, 1);  // Virtualization Exception
	 idt_set_entry(21, (uint64_t)isr21, KERNEL_CS, 0, 0xE, 0, 1);  // Control Protection Exception
	 idt_set_entry(22, (uint64_t)isr22, KERNEL_CS, 0, 0xE, 0, 1);  // Reserved
	 idt_set_entry(23, (uint64_t)isr23, KERNEL_CS, 0, 0xE, 0, 1);  // Hypervisor Injection Exception
	 idt_set_entry(24, (uint64_t)isr24, KERNEL_CS, 0, 0xE, 0, 1);  // VMM Communication Exception
	 idt_set_entry(25, (uint64_t)isr25, KERNEL_CS, 0, 0xE, 0, 1);  // Security Exception
	 idt_set_entry(26, (uint64_t)isr26, KERNEL_CS, 0, 0xE, 0, 1);  // Reserved
	 idt_set_entry(27, (uint64_t)isr27, KERNEL_CS, 0, 0xE, 0, 1);  // Reserved
	 idt_set_entry(28, (uint64_t)isr28, KERNEL_CS, 0, 0xE, 0, 1);  // Reserved
	 idt_set_entry(29, (uint64_t)isr29, KERNEL_CS, 0, 0xE, 0, 1);  // Reserved
	 idt_set_entry(30, (uint64_t)isr30, KERNEL_CS, 0, 0xE, 0, 1);  // Reserved
	 idt_set_entry(31, (uint64_t)isr31, KERNEL_CS, 0, 0xE, 0, 1);  // Reserved
 
	 // -------------------------------
	 // Hardware IRQs (Vectors 32-47)
	 // -------------------------------
	 idt_set_entry(32, (uint64_t)isr32, KERNEL_CS, 0, 0xE, 0, 1);  // IRQ0: Timer/PIT
	 idt_set_entry(33, (uint64_t)isr33, KERNEL_CS, 0, 0xE, 0, 1);  // IRQ1: Keyboard
	 idt_set_entry(34, (uint64_t)isr34, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(35, (uint64_t)isr35, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(36, (uint64_t)isr36, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(37, (uint64_t)isr37, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(38, (uint64_t)isr38, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(39, (uint64_t)isr39, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(40, (uint64_t)isr40, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(41, (uint64_t)isr41, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(42, (uint64_t)isr42, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(43, (uint64_t)isr43, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(44, (uint64_t)isr44, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(45, (uint64_t)isr45, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(46, (uint64_t)isr46, KERNEL_CS, 0, 0xE, 0, 1);
	 idt_set_entry(47, (uint64_t)isr47, KERNEL_CS, 0, 0xE, 0, 1);

	 idt_set_entry(0x50, (uint64_t)uhci_isr80, KERNEL_CS, 0, 0xE, 0, 1);
 
	 // -------------------------------------
	 // System Call Interrupt (Vector 128)
	 // -------------------------------------
	 // Use a trap gate (type 0xF) with DPL=3 so that user space can invoke syscalls.
	 idt_set_entry(128, (uint64_t)isr128, KERNEL_CS, 0, 0xF, 3, 1);
 
	 // Optionally, set a spurious interrupt handler (e.g., vector 255).
	 idt_set_entry(255, (uint64_t)isr_spurious, KERNEL_CS, 0, 0xE, 0, 1);
 
	 // -------------------------------
	 // Load the IDT using the lidt instruction.
	 // -------------------------------
	 idtr_t idtr;
	 idtr.limit = (uint16_t)(sizeof(idt) - 1);
	 idtr.base  = (uint64_t)&idt;
	 __asm__ volatile("lidt %0" : : "m"(idtr));
 
	 kprintf("IDT initialized and loaded. Total entries: %d\n", IDT_ENTRIES);
 }
