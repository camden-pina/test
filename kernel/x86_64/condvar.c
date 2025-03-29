#include <condvar.h>
#include <sched.h>
#include <interrupts/lapic.h>
#include <panic.h>
#include <printf.h>
#include <cpu.h>

/*
 * condvar_wait(cv, m):
 *  - The thread must hold mutex m before calling.
 *  - We put ourselves on cv->wait list, unlock m, schedule, re-lock m on return.
 */

void condvar_init(condvar_t *cv) {
    spinlock_init(&cv->lock);
    cv->wait_head = NULL;
    cv->wait_tail = NULL;
}

void condvar_wait(condvar_t *cv, mutex_t *m) {
    cpu_disable_interrupts();
    spinlock_lock(&cv->lock);

    thread_t *cur = get_current_thread();
    /* add current thread to cv's wait list */
    cur->state = THREAD_BLOCKED;
    cur->wait_next = NULL;
    if (!cv->wait_head) {
        cv->wait_head = cv->wait_tail = cur;
    } else {
        cv->wait_tail->wait_next = cur;
        cv->wait_tail = cur;
    }

    spinlock_unlock(&cv->lock);

    /* now unlock the mutex. We'll block, so we must let others acquire it. */
    mutex_unlock(m);

    /* schedule away */
    schedule_on_cpu(cur->cpu_id);
    cpu_enable_interrupts();

    /* when we are signaled & resumed, re-lock the mutex before returning. */
    mutex_lock(m);
}

void condvar_signal(condvar_t *cv) {
    cpu_disable_interrupts();
    spinlock_lock(&cv->lock);

    if (!cv->wait_head) {
        spinlock_unlock(&cv->lock);
        cpu_enable_interrupts();
        return;
    }

    thread_t *waker = cv->wait_head;
    cv->wait_head = waker->wait_next;
    if (!cv->wait_head)
        cv->wait_tail = NULL;
    waker->wait_next = NULL;
    waker->state = THREAD_READY;
    sched_add_ready_thread(waker, waker->cpu_id);

    spinlock_unlock(&cv->lock);
    cpu_enable_interrupts();

    /* if higher priority, yield. */
    if (waker->priority > get_current_thread()->priority) {
        thread_yield();
    }
}

void condvar_broadcast(condvar_t *cv) {
    cpu_disable_interrupts();
    spinlock_lock(&cv->lock);

    while (cv->wait_head) {
        thread_t *waker = cv->wait_head;
        cv->wait_head = waker->wait_next;
        if (!cv->wait_head)
            cv->wait_tail = NULL;
        waker->wait_next = NULL;
        waker->state = THREAD_READY;
        sched_add_ready_thread(waker, waker->cpu_id);
    }

    spinlock_unlock(&cv->lock);
    cpu_enable_interrupts();

    /* possibly yield if any woken threads have higher priority. */
    thread_yield();
}
