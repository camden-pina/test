#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include "config.h"
#include "thread.h"
#include "spinlock.h"

/*
 * The SMP/Single-core scheduler interface.
 * - Per-CPU run queues
 * - schedule() or schedule_on_cpu()
 * - Timer tick for each CPU
 */

/* Extern global tick if desired */
extern volatile uint64_t g_global_ticks;

/* Initialize the scheduler (for all CPUs or single CPU). */
void sched_init(void);

/* Called on each CPU to set up that CPU's local runqueues, idle thread, etc. */
void sched_init_cpu(int cpu_id);

/* Start scheduling on this CPU (e.g. after booting the CPU). */
void sched_start_cpu(int cpu_id);

/* The main scheduling function for one CPU. 
   Picks the next thread, context-switches away from current if needed. */
void schedule_on_cpu(int cpu_id);

/* Called by the timer interrupt on each CPU. */
void scheduler_tick_this_cpu(int cpu_id);

/* Add a thread to the ready queue of a CPU (public so threads can be enqueued). */
void sched_add_ready_thread(thread_t *t, int cpu_id);

/* Return pointer to idle thread for a CPU */
thread_t* get_idle_thread(int cpu_id);

#endif /* SCHED_H */
