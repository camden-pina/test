#include "sched.h"
#include <interrupts/lapic.h>
#include "printf.h"
#include "panic.h"
#include <mm/pmm.h>    /* kmalloc, kfree */
#include <config.h>
#include <thread.h>
#include <cpu/cpu.h>

/******************************************************************************
 * Per-CPU scheduling structures
 ******************************************************************************/
typedef struct runqueue {
    thread_t   *head;
    thread_t   *tail;
    spinlock_t  lock;  /* protect this queue on SMP */
} runqueue_t;

typedef struct cpu_data {
    thread_t   *current_thread;           /* the thread currently running on this CPU */
    thread_t   *idle_thread;              /* the idle thread for this CPU */
    runqueue_t  runqueues[MAX_PRIORITY];  /* one FIFO queue per priority */
    uint64_t    local_ticks;              /* local tick counter if desired */
} cpu_data_t;

/* Global array of CPU data */
static cpu_data_t g_cpu_data[MAX_CPUS];

/* If you want a global tick counter as well: */
volatile uint64_t g_global_ticks = 0;

/* External context switch routine (assembly or your old code) */
extern void context_switch(context_t *oldctx, context_t *newctx);

/* Forward declarations */
static void init_idle_thread_for_cpu(int cpu_id);
static void idle_thread_func(void *arg);

/* 
 * Return the current thread running on CPU with id 'cpu_id'.
 * In SMP mode, each CPU's current thread is stored in g_cpu_data[cpu_id].current_thread.
 * In single-core mode, cpu_id is always 0.
 */
thread_t* sched_get_current_thread(int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) {
        panic("sched_get_current_thread: Invalid CPU id");
    }
    return g_cpu_data[cpu_id].current_thread;
}

/******************************************************************************
 * schedule_on_cpu(): The main scheduling function (for a single CPU).
 ******************************************************************************/
void schedule_on_cpu(int cpu_id) {
    cpu_data_t *cd = &g_cpu_data[cpu_id];
    thread_t *prev = cd->current_thread;

    /* find highest priority that is non-empty */
    thread_t *next = NULL;
    int found_p = -1;
    for (int p = MAX_PRIORITY - 1; p >= 0; p--) {
        spinlock_lock(&cd->runqueues[p].lock);
        if (cd->runqueues[p].head) {
            /* dequeue head */
            next = cd->runqueues[p].head;
            cd->runqueues[p].head = next->next;
            if (!cd->runqueues[p].head)
                cd->runqueues[p].tail = NULL;
            next->next = NULL;
            spinlock_unlock(&cd->runqueues[p].lock);
            found_p = p;
            break;
        }
        spinlock_unlock(&cd->runqueues[p].lock);
    }
    if (!next) {
        /* fallback to idle */
        next = cd->idle_thread;
    }

    if (next == prev) {
        /* same thread continues */
        return;
    }

    /* Mark next as running */
    next->state = THREAD_RUNNING;
    next->cpu_id = cpu_id;
    next->time_slice = DEFAULT_TIME_SLICE;

    cd->current_thread = next;

    /* If prev was still running, we move it to READY queue (unless it blocked/slept). */
    if (prev && prev->state == THREAD_RUNNING && prev != cd->idle_thread) {
        prev->state = THREAD_READY;
        sched_add_ready_thread(prev, cpu_id);
    }

    if (prev != next) {
        /* context switch */
        context_switch(&prev->context, &next->context);
    }
}

/******************************************************************************
 * sched_add_ready_thread(): public function to add a thread to the CPU's runqueue
 ******************************************************************************/
void sched_add_ready_thread(thread_t *t, int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) {
        panic("sched_add_ready_thread: invalid cpu_id");
    }
    cpu_data_t *cd = &g_cpu_data[cpu_id];

    int p = t->priority;
    if (p < 0) p = 0;
    if (p >= MAX_PRIORITY) p = MAX_PRIORITY - 1;

    t->state = THREAD_READY;
    t->cpu_id = cpu_id;
    t->next = NULL;

    spinlock_lock(&cd->runqueues[p].lock);
    if (!cd->runqueues[p].head) {
        cd->runqueues[p].head = cd->runqueues[p].tail = t;
    } else {
        cd->runqueues[p].tail->next = t;
        cd->runqueues[p].tail = t;
    }
    spinlock_unlock(&cd->runqueues[p].lock);
}

/******************************************************************************
 * scheduler_tick_this_cpu(): called by timer ISR on *this* CPU 
 *  - increments local/global ticks
 *  - checks time slice
 *  - might call schedule_on_cpu(cpu_id) if slice expired
 ******************************************************************************/
void scheduler_tick_this_cpu(int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) return;

    /* global tick increment */
    g_global_ticks++;
    g_cpu_data[cpu_id].local_ticks++;

    /* check current thread's time slice */
    thread_t *curr = g_cpu_data[cpu_id].current_thread;
    if (curr && curr != g_cpu_data[cpu_id].idle_thread) {
        if (curr->time_slice > 0) {
            curr->time_slice--;
            if (curr->time_slice == 0) {
                /* force schedule */
                schedule_on_cpu(cpu_id);
            }
        }
    }

    /* Also, you can check if some threads are sleeping and need to be woken.
       For that, you need a global or per-CPU list of sleeping threads. 
       If a thread's wakeup_tick <= g_global_ticks, you sched_add_ready_thread it. */
}

/******************************************************************************
 * timer_interrupt_handler():
 *  an actual ISR entry point that calls scheduler_tick_this_cpu(get_cpu_id()).
 ******************************************************************************/
void timer_interrupt_handler(void) {
    int cpu_id = get_cpu_id(); /* a function you provide that identifies the local CPU. */
    // send_eoi(); // if using APIC or PIC
    scheduler_tick_this_cpu(cpu_id);
}

/******************************************************************************
 * Idle Thread
 ******************************************************************************/
static void idle_thread_func(void *arg) {
    (void)arg;
    while(1) {
        // On real hardware: __asm__("hlt"); 
        // or just yield
        thread_yield();
    }
}

static void init_idle_thread_for_cpu(int cpu_id) {
    /* Reuse create_kernel_thread but store in g_cpu_data[cpu_id].idle_thread */
    thread_t *idle = create_kernel_thread(idle_thread_func, NULL, "idle", 0);
    if (!idle) panic("Unable to create idle thread");
    g_cpu_data[cpu_id].idle_thread = idle;
}

/******************************************************************************
 * get_idle_thread(cpu_id)
 ******************************************************************************/
thread_t* get_idle_thread(int cpu_id) {
    return g_cpu_data[cpu_id].idle_thread;
}

/******************************************************************************
 * sched_init(): init global data, should be called once on BSP
 ******************************************************************************/
void sched_init(void) {
    kprintf("sched_init: Setting up SMP scheduler\n");
    for (int c = 0; c < MAX_CPUS; c++) {
        for (int p = 0; p < MAX_PRIORITY; p++) {
            g_cpu_data[c].runqueues[p].head = NULL;
            g_cpu_data[c].runqueues[p].tail = NULL;
            spinlock_init(&g_cpu_data[c].runqueues[p].lock);
        }
        g_cpu_data[c].current_thread = NULL;
        g_cpu_data[c].idle_thread    = NULL;
        g_cpu_data[c].local_ticks    = 0;
    }
    g_global_ticks = 0;
}

/******************************************************************************
 * sched_init_cpu(cpu_id): 
 *  called for each CPU (including BSP=0) to set up that CPU's data structures 
 *  and create an idle thread, etc.
 ******************************************************************************/
void sched_init_cpu(int cpu_id) {
    init_idle_thread_for_cpu(cpu_id);
    /* Create a dummy 'boot' thread as the current_thread if you like. */
    static thread_t boot_thread[MAX_CPUS];
    boot_thread[cpu_id].tid      = -1;
    boot_thread[cpu_id].priority = 0;
    boot_thread[cpu_id].state    = THREAD_RUNNING;
    boot_thread[cpu_id].cpu_id   = cpu_id;
    g_cpu_data[cpu_id].current_thread = &boot_thread[cpu_id];

    kprintf("sched_init_cpu(%d): done.\n", cpu_id);
}

/******************************************************************************
 * sched_start_cpu(cpu_id):
 *  optionally call schedule_on_cpu(cpu_id) if you want to start scheduling 
 *  from the 'boot_thread' to something else. 
 ******************************************************************************/
void sched_start_cpu(int cpu_id) {
    schedule_on_cpu(cpu_id);
}
