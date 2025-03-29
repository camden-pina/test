#include "spinlock.h"
#include <cpu.h>  /* disable_interrupts(), enable_interrupts() */
#include "config.h"

/* 
 * A simple implementation:
 *  - In SMP mode, we do an atomic compare/exchange in a loop 
 *    and also disable local interrupts to prevent preemption.
 *  - In single-core (MAX_CPUS=1) you could do just disable_interrupts(),
 *    but let's unify so that if SMP is off, we still do the same 
 *    (the cost is minor).
 */

void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

/* Acquire spinlock */
void spinlock_lock(spinlock_t *lock) {
#ifdef SMP
    cpu_disable_interrupts();  // prevent local CPU preemption
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        // spin
    }
#else
    /* Single-core fallback: just disable interrupts is enough. 
       Another approach is the same code. 
    */
    cpu_disable_interrupts();
    while (lock->locked) {
        /* if single-core but an interrupt tries to lock, 
           we can deadlock. Typically single-core kernel 
           does not use spinlocks, just int disable. 
           We'll unify to match SMP logic. 
        */
        // spin 
    }
    lock->locked = 1;
#endif
}

/* Release spinlock */
void spinlock_unlock(spinlock_t *lock) {
    /* Mark unlocked. On x86, __sync_lock_release or store 0. */
#ifdef SMP
    lock->locked = 0;
    __sync_synchronize(); // memory barrier (optional)
    cpu_enable_interrupts();
#else
    lock->locked = 0;
    cpu_enable_interrupts();
#endif
}
