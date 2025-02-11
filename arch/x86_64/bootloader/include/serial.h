#ifndef SERIAL_H
#define SERIAL_H

/* Initialize the serial port (for early debug output, if needed) */
void serial_init(void);

/* Output a single character via the serial port */
void serial_putc(char c);

/* Print a null‑terminated string to the serial port */
void serial_print(const char* str);

#endif // SERIAL_H

