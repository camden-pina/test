#include <descriptor_tables/isr.h>
#include <interrupts/lapic.h>

#include <stdio.h>

static void (*interrupts_handlers[256])(int);

// Register a defined function with as a handler for a certain interrupt or exception number
void register_interrupt_handler(unsigned int irq, void (*handler)(void)) {
	if (irq <= 256)
		interrupts_handlers[irq] = (void (*)(int))handler; // added (int *) 2/10/2025 7:59pm
}

void isr_common(unsigned char num, int error_code)
{
	if (num <= 0x20)
	{
		/* AN EXCEPTION HAS OCCURED
		Clear the screen
		Print error code (if available)
		Halt the system
		*/

		krnl_printf_reset_x();
		krnl_printf_reset_y();
		drawRect(0, 0, 1366, 768, 0xFF0000FF);
		
		printf("\n\rAN EXCEPTION HAS OCCURED: %08%x\n", num);
		printf("\rError Code: %08%x\n\r", error_code);
		printf("Error Code: %08%x", error_code);
		asm volatile("cli");
		asm volatile("hlt");
	}
	interrupts_handlers[num](error_code);	// Call interrupt handler
	apic_send_eoi_if_necessary(num);		// Send EOI
}
