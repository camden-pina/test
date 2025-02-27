#include <kernel.h>
#include <descriptor_tables/gdt.h>
#include <descriptor_tables/idt.h>
#include <descriptor_tables/isr.h>
#include <interrupts/lapic.h>
#include <acpi/acpi.h>
#include <mm/pmm.h>
#include <mm/vmem.h>

#include "ps2_mouse.h"

#include <stdarg.h>

#include <io.h>
#include <string.h>

static unsigned char SCAN_CODE_MAPPING[] = "\x00""\x1B""1234567890-=""\x08""\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 \0\0\0\0\0\0\0\0\0\0\0\0\0-456+1230.\0\0\0\0\0";

static void keyboard_isr(void)
{
	unsigned char scancode = inb(0x60);	// Read Scancode

	// Reset Keyboard Controller
	unsigned char a = inb(0x61);
	a |= 0x82;
	outb(0x61, a);
	a &= 0x7f;
	outb(0x61, a);

	if (scancode & 0x80)
	{
		scancode = scancode & ~0x80;

		unsigned char pressed_char = SCAN_CODE_MAPPING[scancode];

		// putc(pressed_char);
	}
}

static void ps2_keyboard_init(void)
{
	kprintf("Initializing Keyboard\n\r");
	register_interrupt_handler(33, keyboard_isr);
	ioapic_map(1, 33);
}

extern uint64_t kernel_start;
extern uint64_t kernel_end;

// Inline functions to perform port I/O operations.
static inline void serial_outb(uint16_t port, uint8_t data) {
    __asm__ volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint8_t serial_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Initializes the serial port (COM1) for output.
// This configuration sets COM1 to 38400 baud, 8N1 with FIFO enabled.
void init_serial(void) {
    // Disable interrupts on the serial port.
    serial_outb(0x3F8 + 1, 0x00);

    // Enable Divisor Latch Access Bit (DLAB) to set baud rate divisor.
    serial_outb(0x3F8 + 3, 0x80);

    // Set divisor to 3 (low byte) for 38400 baud.
    serial_outb(0x3F8 + 0, 0x03);

    // Set divisor (high byte) to 0.
    serial_outb(0x3F8 + 1, 0x00);

    // Disable DLAB and configure: 8 bits, no parity, one stop bit.
    serial_outb(0x3F8 + 3, 0x03);

    // Enable FIFO, clear them, with 14-byte threshold.
    serial_outb(0x3F8 + 2, 0xC7);

    // Enable interrupts (if desired) and set RTS/DSR.
    serial_outb(0x3F8 + 4, 0x0B);
}

// Sends a single character to the serial port.
void serial_putc(char c) {
    // Wait until the Transmit Holding Register is empty.
    while (!(serial_inb(0x3F8 + 5) & 0x20))
        ;
    serial_outb(0x3F8, c);
}

// Writes a null-terminated string to the serial port.
void serial_write(const char *str) {
    while (*str) {
        serial_putc(*str++);
    }
}

void kern_main(flexboot_header_t* boot_hdr)
{
	init_serial();
	serial_putc('x');

	// krnl_set_graphics_ouutput_protocol(boot_hdr->fb->base_addr, boot_hdr->fb->px_width, boot_hdr->fb->px_height, boot_hdr->fb->bpp, boot_hdr->fb->pps);
    // drawRect(0, 0, 100, 200, 0xAAAAFFFF);
	serial_putc('f');
	
	pmm_init(boot_hdr->memoryMap, boot_hdr->mapSize, boot_hdr->descriptorSize);

	uint64_t kernel_size = (uint64_t)&kernel_end - (uint64_t)&kernel_start;
	uint64_t kernel_pages = (uint64_t)(kernel_size / 0x1000) + 1;
	
	pmm_pages_lock(&kernel_start, kernel_pages);

	vmem_init((uint64_t)boot_hdr->fb->base_addr, boot_hdr->fb->buffer_sz);
	
	kprintf("\n\rtotal_memory: %l", pmm_get_total_memory());
	kprintf("\n\rtotal_memory_used: %l", pmm_get_total_memory_used());
	kprintf("\n\rtotal_memory_free: %l", pmm_get_total_memory_free());
	kprintf("\n\rtotal_memory_reserved: %l", pmm_get_total_memory_reserved());
	
	__asm__ volatile("cli");
	acpi_init(boot_hdr->rsdp_addr);
	gdt_init();
	apic_init();
	idt_init();

	ps2_keyboard_init();
	ps2_mouse_init();
	__asm__ volatile("sti");
	// drawRect(0, 0, boot_hdr->fb->px_width, boot_hdr->fb->px_height, 0x00000000);
	// krnl_printf_reset_x();
	// krnl_printf_reset_y();
	
	kprintf("ModernOS (C)\n\n\r");
	kprintf("Copyright (C) Ideal Technologies Inc.\n\r");
	
	kprintf("\n\rfree_memory: %08%x\n\r", pmm_get_total_memory_free());
	
	while (1)
	{

	}
}