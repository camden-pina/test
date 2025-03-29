#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include "config.h"

/* Thread states */
typedef enum {
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_BLOCKED,
    THREAD_SLEEPING,
    THREAD_TERMINATED
} thread_state_t;

/* x86_64 CPU context for switching */
typedef struct context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rip;
    /* real code might also store rflags, etc. */
} context_t;

/* Forward declaration */
struct thread_t;
typedef void (*thread_func_t)(void *);

/* Thread Control Block */
typedef struct thread_t {
    struct thread_t *next;   /* for runqueues, wait lists, etc. */

    int              tid;
    char             name[32];
    thread_state_t   state;
    int              priority;    /* 0..MAX_PRIORITY-1 */
    int              cpu_id;      /* which CPU this thread is running on */
    uint64_t         wakeup_tick; /* for sleeping */
    uint64_t         time_slice;  /* remaining quantum */

    /* stack + context */
    void            *stack_base;
    context_t        context;

    /* wait_next used in mutex/condvar wait lists */
    struct thread_t *wait_next;
} thread_t;

/* Public thread API */
thread_t* create_kernel_thread(thread_func_t func, void *arg, 
                               const char *name, int priority);
void      thread_yield(void);
void      thread_exit(void);
void      thread_sleep(uint64_t ticks);

/* You might want a function to get the current thread pointer. */
thread_t* get_current_thread(void);

#endif /* THREAD_H */
