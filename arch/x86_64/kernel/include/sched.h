#ifndef KERNEL_SCHED_H
#define KERENL_SCHED_H

#include <kernel.h>
#include <proc.h>
#include <tqueue.h>

typedef enum sched_reason {
    SCHED_UPDATED,
    SCHED_YIELDED,
    SCHED_BLOCKED,
    SCHED_SLEEPING,
    SCHED_PREEMPTED,
    SCHED_EXITED,
  } sched_reason_t;

void sched_init();

void sched_again(sched_reason_t readon);

#endif
