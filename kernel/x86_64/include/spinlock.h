#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

/*
 * A simple spinlock that uses atomic test-and-set or exchange.
 * On x86_64, you can use __sync_lock_test_and_set or __atomic builtins.
 * We also may disable/enable interrupts to avoid local preemption 
 * if you want to ensure lock holder isn't preempted on single core.
 */

typedef struct {
    volatile int locked;
} spinlock_t;

void spinlock_init(spinlock_t *lock);
void spinlock_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);

#endif /* SPINLOCK_H */
