#ifndef _IO_H
#define _IO_H 1

unsigned long long read_msr(unsigned long long index);
void write_msr(unsigned long long index, unsigned long long value);
unsigned char io_read_8(unsigned port);
void io_write_8(unsigned port, unsigned char val);
void outl(unsigned short port, unsigned int value);
unsigned int inl(unsigned short port);
void outw(unsigned short port, unsigned short value);
unsigned short inw(unsigned short port);
void outb(unsigned short port, unsigned char value);
unsigned char inb(unsigned short port);

#endif
