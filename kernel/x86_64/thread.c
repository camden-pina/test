#include "thread.h"
#include "sched.h"
#include <interrupts/lapic.h>
#include "mm/pmm.h"
#include "panic.h"
#include "printf.h"
#include "config.h"
#include <cpu/cpu.h>

static int global_tid_counter = 1;

/* We define a small structure to pass (func,arg) to the new thread. */
typedef struct thread_startup {
    thread_func_t fn;
    void         *arg;
} thread_startup_t;

/* We'll define a stub to pop thread_startup_t off the stack and run it. */
__attribute__((noreturn)) static void thread_stub(void) {
    thread_startup_t *startup;
    __asm__ volatile("pop %0" : "=r"(startup));
    if (!startup) {
        kprintf("thread_stub: got NULL startup\n");
        while(1);
    }
    startup->fn(startup->arg);
    kprintf("Thread function returned, calling thread_exit.\n");
    thread_exit();
    while(1); /* never reached */
}

/* allocate a TCB + stack, set initial context. */
static thread_t* allocate_thread(const char *name, int priority) {
    thread_t *t = kmalloc(sizeof(thread_t));
    if (!t) panic("allocate_thread: out of memory for TCB");
    for (size_t i=0; i<sizeof(*t); i++) ((char*)t)[i] = 0;

    t->tid      = global_tid_counter++;
    t->priority = priority;
    t->state    = THREAD_READY;
    t->stack_base = kmalloc(STACK_SIZE);
    if (!t->stack_base) panic("allocate_thread: out of memory for stack");

    /* copy thread name if provided */
    if (name) {
        int i=0; 
        while(name[i] && i<31) { t->name[i] = name[i]; i++; }
        t->name[i] = 0;
    }

    /* prepare stack: put a fake return address + thread_startup pointer. */
    uint64_t *stk = (uint64_t*)((char*)t->stack_base + STACK_SIZE);
    /* align to 16 bytes */
    stk = (uint64_t*)((uintptr_t)stk & ~0xF);
    /* fake return address */
    *(--stk) = 0;
    t->context.rip = (uint64_t)thread_stub;
    t->context.rsp = (uint64_t)stk;

    return t;
}

/* create_kernel_thread: public function to create a kernel thread */
thread_t* create_kernel_thread(thread_func_t func, void *arg, 
                               const char *name, int priority)
{
    kprintf("create_kernel_thread: name=%s prio=%d\n", name?name:"(null)", priority);
    thread_t *t = allocate_thread(name, priority);

    /* place a thread_startup_t on the new stack */
    thread_startup_t *startup = kmalloc(sizeof(thread_startup_t));
    if (!startup) panic("no mem for thread_startup");
    startup->fn  = func;
    startup->arg = arg;

    /* push pointer onto stack */
    uint64_t *st = (uint64_t*)t->context.rsp;
    *(--st) = (uint64_t)startup;
    t->context.rsp = (uint64_t)st;

    /* add to ready queue of whichever CPU you want. 
       For now, let's add to CPU0 or pick e.g. get_cpu_id(). 
       If you want better SMP load balancing, you could pick the least loaded CPU. 
    */
    int cpu_id = 0;  /* or get_cpu_id(), or some policy */
    sched_add_ready_thread(t, cpu_id);

    return t;
}

/* Helper to get current CPU's current thread. We rely on sched.c's data. */
extern thread_t* get_current_thread_on_cpu(int cpu_id); 
/* We'll define get_current_thread() by calling a function in sched that returns 
   g_cpu_data[cpu_id].current_thread, or we do it ourselves. 
*/

thread_t* get_current_thread(void) {
    int c = get_cpu_id();
    extern thread_t* sched_get_current_thread(int cpu_id);
    return sched_get_current_thread(c);
}

/* yield: voluntarily give up CPU. */
void thread_yield(void) {
    cpu_disable_interrupts();
    thread_t *cur = get_current_thread();
    if (cur->state == THREAD_RUNNING) {
        cur->state = THREAD_READY;
        sched_add_ready_thread(cur, cur->cpu_id);
    }
    schedule_on_cpu(cur->cpu_id);
    cpu_enable_interrupts();
}

/* exit: mark self as terminated, schedule away. */
void thread_exit(void) {
    cpu_disable_interrupts();
    thread_t *cur = get_current_thread();
    kprintf("thread_exit: TID=%d\n", cur->tid);
    cur->state = THREAD_TERMINATED;

    /* free resources, but be careful: can't free stack while still in use. 
       Typically you do a reaper or let the idle thread do final cleanup. 
    */
    schedule_on_cpu(cur->cpu_id);
    cpu_enable_interrupts();
    panic("thread_exit: returned from schedule_on_cpu");
}

/* sleep: block for 'ticks'. We'll store wakeup_tick and schedule away. */
extern volatile uint64_t g_global_ticks;
void thread_sleep(uint64_t ticks) {
    cpu_disable_interrupts();
    thread_t *cur = get_current_thread();
    cur->wakeup_tick = g_global_ticks + ticks;
    cur->state = THREAD_SLEEPING;
    schedule_on_cpu(cur->cpu_id);
    cpu_enable_interrupts();
}
