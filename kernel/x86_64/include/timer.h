#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Returns the current time in milliseconds */
uint32_t get_time_ms(void);

/* Yields the CPU to allow other tasks/interrupts to run */
void yield(void);

#endif // TIMER_H
