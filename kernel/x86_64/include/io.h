#ifndef IO_H
#define IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Output a byte to a specified port.
 * Parameters:
 *   port - The I/O port address.
 *   data - The 8-bit data to output.
 */
void outb(unsigned int port, unsigned char data);

/* 
 * Input a byte from a specified port.
 * Parameters:
 *   port - The I/O port address.
 * Returns:
 *   The 8-bit data read from the port.
 */
unsigned char inb(unsigned int port);

/* 
 * Output a word (16 bits) to a specified port.
 * Parameters:
 *   port - The I/O port address.
 *   data - The 16-bit data to output.
 */
void outw(unsigned int port, unsigned short data);

/* 
 * Input a word (16 bits) from a specified port.
 * Parameters:
 *   port - The I/O port address.
 * Returns:
 *   The 16-bit data read from the port.
 */
unsigned short inw(unsigned int port);

/* 
 * Output a double word (32 bits) to a specified port.
 * Parameters:
 *   port - The I/O port address.
 *   data - The 32-bit data to output.
 */
void outdw(unsigned int port, unsigned int data);

/* 
 * Input a double word (32 bits) from a specified port.
 * Parameters:
 *   port - The I/O port address.
 * Returns:
 *   The 32-bit data read from the port.
 */
unsigned int indw(unsigned int port);

/* 
 * Output a double word (32 bits) to a specified port.
 * This function is similar to outdw.
 * Parameters:
 *   port - The I/O port address.
 *   data - The 32-bit data to output.
 */
void outl(unsigned int port, unsigned int data);

/* 
 * Input a double word (32 bits) from a specified port.
 * This function is similar to indw.
 * Parameters:
 *   port - The I/O port address.
 * Returns:
 *   The 32-bit data read from the port.
 */
unsigned int inl(unsigned int port);

/* 
 * Read a Model Specific Register (MSR).
 * Parameters:
 *   index - The MSR index to read.
 * Returns:
 *   The 64-bit value of the specified MSR.
 */
unsigned long long read_msr(unsigned long long index);

/* 
 * Write to a Model Specific Register (MSR).
 * Parameters:
 *   index - The MSR index to write.
 *   value - The 64-bit value to write.
 */
void write_msr(unsigned long long index, unsigned long long value);

/* 
 * Read an 8-bit value from a specified I/O port.
 * Parameters:
 *   port - The I/O port address.
 * Returns:
 *   The 8-bit data read from the port.
 */
unsigned char io_read_8(unsigned int port);

/* 
 * Write an 8-bit value to a specified I/O port.
 * Parameters:
 *   port - The I/O port address.
 *   value - The 8-bit value to write.
 */
void io_write_8(unsigned int port, unsigned char value);

#ifdef __cplusplus
}
#endif

#endif /* IO_H */
