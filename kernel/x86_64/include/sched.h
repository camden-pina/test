#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "task.h"
#include <spinlock.h>

/* Task state flags */
#define TASK_RUNNING   0  /* Currently running on a CPU */
#define TASK_READY     1  /* Ready to run, on a runqueue */
#define TASK_BLOCKED   2  /* Waiting (sleeping or on I/O) */
#define TASK_ZOMBIE    3  /* Terminated, not yet reaped */

/* Simple per-CPU runqueue structure */
struct runqueue {
   spinlock_t lock;
    struct task_struct *head;  /* Circular linked list of runnable tasks */
    struct task_struct *curr;  /* Current running task on this CPU */
} __attribute__((packed));

/* Initialize scheduler and idle task for each CPU */
void scheduler_init(void);

/* Preemptive scheduler tick handler (called from timer IRQ) */
void *schedule_irq(void *context);

/* Voluntary context switch (e.g., yield or blocking) */
void schedule(void);

/* Add a task to a CPU runqueue (for a specific CPU or current CPU) */
void enqueue_task(struct task_struct *task, int cpu);

/* Remove a task from its runqueue (if not running) */
void dequeue_task(struct task_struct *task, int cpu);

/* Global runqueue array (one per CPU) */
extern struct runqueue cpu_runqueue[];

/* Current CPU id getter (stub implementation) */
int get_cpu_id(void);  /* returns the current CPU index (0 if single-core) */

void rq_lock(int cpu);
void rq_unlock(int cpu);

#endif  /* SCHEDULER_H */
