#include <sys/io.h>

unsigned long long read_msr(unsigned long long index) {
	unsigned long long high, low;
	__asm__("rdmsr" : "=a" (low), "=d" (high) : "c" (index));

	return high << 32 | low;
}

void write_msr(unsigned long long index, unsigned long long value) {
	unsigned long long high = value >> 32;
	unsigned long long low = (value & 0x0000000000000000ffffffffffffffff);
	__asm__("rdmsr" : : "a" (low), "d" (high), "c" (index));
}

unsigned char io_read_8(unsigned port) {
	unsigned char ret;
	__asm__ volatile ("inb %w1, %b0" : "=a" (ret) : "Nd" (port));
	return ret;
}

void io_write_8(unsigned port, unsigned char val) {
	__asm__ volatile ("outb %b0, %w1" : : "a" (val), "Nd" (port));
}

/*
 * Output doubleword to I/O port
 */
void outl(unsigned short port, unsigned int value) {
	__asm__ volatile("outl %%eax, %%dx" : : "a" (value), "d" (port));
}

/*
 * Read doubleword from I/O port
 */
unsigned int inl(unsigned short port) {
	unsigned int value;
	__asm__ volatile("inl %%dx, %%eax" : "=a" (value) : "d" (port));
	return value;
}

/*
 * Output word to I/O port
 */
void outw(unsigned short port, unsigned short value) {
	__asm__ volatile("outw %%ax, %%dx" : : "a" (value), "d" (port));
}

/*
 * Read word from I/O port
 */
unsigned short inw(unsigned short port) {
	unsigned short value;
	__asm__ volatile("inw %%dx, %%ax" : "=a" (value) : "d" (port));
	return value;
}

/*
 * Output byte to I/O port
 */
void outb(unsigned short port, unsigned char value) {
	__asm__ volatile("outb %%al, %%dx" : : "a" (value), "d" (port));
}

/*
 * Read byte from I/O port
 */
unsigned char inb(unsigned short port) {
	unsigned char value;
	__asm__ volatile("inb %%dx, %%al" : "=a" (value) : "d" (port));
	return value;
}
