#ifndef LOG_H
#define LOG_H

#include <stddef.h>

#define TTY_MAX_ROWS    1024
#define TTY_ROW_MAX_LEN 256

// Initialize the logging subsystem.
void log_init(void);

// Append a null–terminated string to the log buffer.
void log_print_str(const char *str);

// Append a single character to the log buffer.
void log_print_char(char ch);

// Retrieve the log row at the given index.
const char* log_get_row(int index);

// Retrieve the current number of rows in the log.
int log_get_row_count(void);

#endif
