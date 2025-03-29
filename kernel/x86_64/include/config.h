#ifndef CONFIG_H
#define CONFIG_H

/* 
 * Basic configuration constants for your kernel and SMP.
 * Adjust as needed.
 */

/* If you want SMP, define this. Otherwise comment it out for single-core. */
// #define SMP 1

#define MAX_CPUS  4  /* 1..N. If 1, effectively single-core. */
#define MAX_PRIORITY 32  /* 0..31, 31=highest */

#define DEFAULT_TIME_SLICE 5
#define STACK_SIZE (64 * 1024)

#endif /* CONFIG_H */
