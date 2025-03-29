#ifndef MUTEX_H
#define MUTEX_H

#include "spinlock.h"
#include "thread.h"

/* A sleeping mutex with a spinlock for SMP safety. */

typedef struct mutex {
    spinlock_t lock;
    int        locked;      /* 0=unlocked, 1=locked */
    thread_t  *owner;
    thread_t  *wait_head;
    thread_t  *wait_tail;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif /* MUTEX_H */
