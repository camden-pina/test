#include <timer.h>
#include <stdint.h>

/*
 * Global variable to store the TSC frequency (in Hz).
 * This value should be calibrated during system initialization.
 * For example, for a 3 GHz CPU:
 */
uint64_t tsc_frequency = 3000000000ULL;  // Example: 3 GHz

/*
 * rdtsc:
 * Reads the CPU Time Stamp Counter (TSC) using AT&T inline assembly.
 * The code below uses GCC's default AT&T syntax.
 */
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" 
                  : "=a" (lo), "=d" (hi)   /* Outputs: lower 32 bits in %eax, higher 32 bits in %edx */
                  :                       /* No inputs */
                  : );                   /* No clobbers */
    return ((uint64_t)hi << 32) | lo;
}

/*
 * get_time_ms:
 * Returns the current time in milliseconds by converting the TSC value.
 * It reads the TSC and divides by the frequency (converted to ms).
 */
uint32_t get_time_ms(void) {
    uint64_t tsc = rdtsc();
    // Multiply by 1000 to get milliseconds, then divide by tsc_frequency.
    return (uint32_t)((tsc * 1000) / tsc_frequency);
}

/*
 * yield:
 * A simple implementation that issues the PAUSE instruction.
 * This inline assembly uses AT&T syntax.
 * In a full OS, this would interact with the scheduler to perform a context switch.
 */
void yield(void) {
    __asm__ volatile ("pause" ::: "memory");
}
