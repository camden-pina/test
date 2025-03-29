#ifndef CONDVAR_H
#define CONDVAR_H

#include <mutex.h>

typedef struct condvar {
    spinlock_t lock;
    thread_t  *wait_head;
    thread_t  *wait_tail;
} condvar_t;

void condvar_init(condvar_t *cv);
void condvar_wait(condvar_t *cv, mutex_t *m);
void condvar_signal(condvar_t *cv);
void condvar_broadcast(condvar_t *cv);

#endif /* CONDVAR_H */
