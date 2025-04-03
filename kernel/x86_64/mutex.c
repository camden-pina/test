#include <mutex.h>
#include <sched.h>
#include <interrupts/lapic.h>
#include "panic.h"
#include "printf.h"
#include <cpu/cpu.h>

/*
 * Implementation:
 *  - We embed a spinlock to protect the fields in the mutex (locked, owner, wait list).
 *  - If locked by someone else, we block the current thread in a wait queue.
 *  - On unlock, we wake one waiter, giving ownership to it.
 */

void mutex_init(mutex_t *m) {
    spinlock_init(&m->lock);
    m->locked    = 0;
    m->owner     = NULL;
    m->wait_head = NULL;
    m->wait_tail = NULL;
}

void mutex_lock(mutex_t *m) {
    cpu_disable_interrupts();
    spinlock_lock(&m->lock);

    if (!m->locked) {
        /* acquire lock */
        m->locked = 1;
        m->owner  = get_current_thread();
        spinlock_unlock(&m->lock);
        cpu_enable_interrupts();
        return;
    }

    /* if already locked by me, either allow recursion or panic. We'll panic. */
    if (m->owner == get_current_thread()) {
        spinlock_unlock(&m->lock);
        cpu_enable_interrupts();
        panic("mutex_lock: already locked by current thread");
    }

    /* block current thread on wait list */
    thread_t *cur = get_current_thread();
    cur->state = THREAD_BLOCKED;
    cur->wait_next = NULL;

    if (!m->wait_head) {
        m->wait_head = m->wait_tail = cur;
    } else {
        m->wait_tail->wait_next = cur;
        m->wait_tail = cur;
    }

    spinlock_unlock(&m->lock);

    /* schedule away */
    schedule_on_cpu(cur->cpu_id);

    cpu_enable_interrupts();

    /* when we resume, we own the mutex (the unlocker gave it to us) */
}

void mutex_unlock(mutex_t *m) {
    cpu_disable_interrupts();
    spinlock_lock(&m->lock);

    thread_t *cur = get_current_thread();
    if (m->owner != cur) {
        spinlock_unlock(&m->lock);
        cpu_enable_interrupts();
        panic("mutex_unlock: current thread does not own this mutex!");
    }

    /* if there are waiters, pass ownership to the first. */
    if (m->wait_head) {
        thread_t *waker = m->wait_head;
        m->wait_head = waker->wait_next;
        if (!m->wait_head)
            m->wait_tail = NULL;
        m->owner = waker;
        /* still locked=1, just new owner. */
        waker->state = THREAD_READY;
        sched_add_ready_thread(waker, waker->cpu_id);

        /* priority inheritance undone if you do that. Omitted here. */

        spinlock_unlock(&m->lock);
        cpu_enable_interrupts();

        /* if waker is higher priority, yield. */
        if (waker->priority > cur->priority) {
            thread_yield();
        }
    } else {
        /* no waiters => fully unlock */
        m->locked = 0;
        m->owner  = NULL;
        spinlock_unlock(&m->lock);
        cpu_enable_interrupts();
    }
}
