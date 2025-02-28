#include "ps2_mouse.h"
#include <printf.h>

#define MOUSE_STATUS 0x64
#define MOUSE_PORT 0x60

static unsigned int mouse_x = 0;
static unsigned int mouse_y = 0;
static char mouse_byte[3];
static unsigned char mouse_cycle = 0;

static void mouse_handler(void)
{
	char _status = inb(MOUSE_PORT);

	switch (mouse_cycle)
	{
		case 0:
		{
			mouse_byte[0] = _status;
			mouse_cycle++;
			break;
		}
		case 1:
		{
			mouse_byte[1] = _status;
			mouse_cycle++;
			break;
		}
		case 2:
		{
			mouse_byte[2] = _status;

			if (mouse_byte[0] & ( 1 << 6) || mouse_byte[0] & ( 1 << 7))	// Overflow
				break;
			
			mouse_x += mouse_byte[1];
			mouse_y -= mouse_byte[2];

			// drawRect(mouse_x, mouse_y, 10, 10, 0xFFFF0000);
			mouse_cycle = 0;
			break;
		}
	}
}

static void mouse_wait(unsigned char a_type) {
	unsigned int _time_out = 100000; //unsigned int
	if (a_type == 0) {
		while (_time_out--) {
			if ((inb(0x64) & 1) == 1)
			{
				return;
			}
		}
		return;
	}
	else {
		while (_time_out--) {
			if ((inb(0x64) & 2) == 0) {
				return;
			}
		}
		return;
	}
}

static void mouse_write(unsigned char a_write) //unsigned char
{
	//Tell the mouse we are sending a command
	mouse_wait(1);
	outb(0x64, 0xD4);
	mouse_wait(1);
	//Finally write
	outb(0x60, a_write);
}

static unsigned char mouse_read(void)
{
	mouse_wait(0);
	return inb(0x60);
}

unsigned long long get_mouse_x(void) { return mouse_x; }
unsigned long long get_mouse_y(void) { return mouse_y; }

void ps2_mouse_init(void)
{
	kprintf("Initializing Mouse");
	mouse_x = 200;
	mouse_y = 200;
	mouse_cycle = 0;

	unsigned char _status;  //unsigned char
	//Enable the auxiliary mouse device
	mouse_wait(1);
	outb(0x64, 0xA8);

	//Enable the interrupts
	mouse_wait(1);
	outb(0x64, 0x20);
	mouse_wait(0);
	_status = (inb(0x60) | 2);
	mouse_wait(1);
	outb(0x64, 0x60);
	mouse_wait(1);
	outb(0x60, _status);

	// Tell the mouse to use default settings
	mouse_write(0xF6);
	mouse_read();  //Acknowledge

	// Enable the mouse
	mouse_write(0xF4);
	mouse_read();  //Acknowledge

	// Setup mouse handler
	register_interrupt_handler(44, mouse_handler);
	ioapic_map(12, 44);
}
