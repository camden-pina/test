#include <descriptor_tables/isr.h>
#include <interrupts/lapic.h>
#include <printf.h>
#include <panic.h>

static void (*interrupts_handlers[256])(int);

// Register a defined function with as a handler for a certain interrupt or exception number
void register_interrupt_handler(unsigned int irq, void (*handler)(void)) {
	if (irq <= 256)
		interrupts_handlers[irq] = (void (*)(int))handler; // added (int *) 2/10/2025 7:59pm
}

char *exceptions[] = {
	"Division Error",
	"Debug",
	"Non-Maskable Interrupt",
	"Breakpoint",
	"Overflow",
	"Bound Range Exceeded",
	"Invalid Opcode",
	"Device Not Available",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Invalid TSS",
	"Segment Not Present",
	"Stack-Segment Fault",
	"General Protection Fault",
	"Page Fault",
	"Reserved1",
	"x87 Floating-Point Exception",
	"Alignment Check",
	"Machine Check",
	"SIMD Floating-Point Exception",
	"Virtualiation Exception",
	"Control Protection Exception",
	"Reserved2",
	"Hypervisor Injection Exception",
	"VMM Communication Exception",
	"Security Exception",
	"Reserved3",
	"Triple Fault",
	"FPU Error Interrupt"
};

void isr_common(unsigned char num, int error_code)
{
	if (num <= 0x20)
	{
		/* AN EXCEPTION HAS OCCURED
		Clear the screen
		Print error code (if available)
		Halt the system
		*/

		// krnl_printf_reset_x();
		// krnl_printf_reset_y();
		// drawRect(0, 0, 1366, 768, 0xFF0000FF);
		
		kprintf("\n\rAN EXCEPTION HAS OCCURED: %s\n", exceptions[num]);
		kprintf("\rError Code: 0x%x\n\r", error_code);
		__asm__ volatile("cli");
		__asm__ volatile("hlt");
	}
	interrupts_handlers[num](error_code);	// Call interrupt handler
	apic_send_eoi_if_necessary(num);		// Send EOI
}
