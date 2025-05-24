#include "sched.h"
#include "task.h"
#include "syscall.h"
#include "context.h"
#include <stddef.h>  // for NULL
#include <descriptor_tables/gdt.h>

/* Assume a fixed small number of CPUs (e.g., 1 for simplicity or small SMP) */
#define MAX_CPUS  4

/* Per-CPU runqueue array */
struct runqueue cpu_runqueue[MAX_CPUS];

/* Acquire the runqueue lock for the given CPU */
inline void rq_lock(int cpu) {
    spinlock_lock(&cpu_runqueue[cpu].lock);
}

/* Release the runqueue lock for the given CPU */
inline void rq_unlock(int cpu) {
    spinlock_unlock(&cpu_runqueue[cpu].lock);
}

/* Get current CPU ID (stub: always 0 in single-core default) */
int get_cpu_id(void) {
    return 0;
}

void scheduler_init(void) {
    int cpu;
    
    // Initialize spinlock for each CPU's runqueue.
    for (cpu = 0; cpu < MAX_CPUS; cpu++) {
        spinlock_init(&cpu_runqueue[cpu].lock);
        // Initialize other runqueue fields to safe defaults.
        cpu_runqueue[cpu].head = NULL;
        cpu_runqueue[cpu].curr = NULL;
    }
    
    // Get current CPU id (stub: returns 0 in single-core system).
    int cpu_id = get_cpu_id();
    
    // Lock the runqueue for the current CPU.
    rq_lock(cpu_id);
    
    // Create and set up the idle task.
    struct task_struct *idle = create_idle_task(cpu_id);
    idle->run_list.next = idle;
    idle->run_list.prev = idle;
    cpu_runqueue[cpu_id].head = idle;
    cpu_runqueue[cpu_id].curr = idle;
    idle->state = TASK_RUNNING;
    current_task = idle;
    
    // Create initial user process ("/init") and enqueue it.
    struct task_struct *init_task = create_user_process("/init", NULL, NULL);
    if (init_task) {
        init_task->state = TASK_READY;
        enqueue_task(init_task, cpu_id);
    }
    
    // Unlock the runqueue for the current CPU.
    rq_unlock(cpu_id);
    
    // Immediately schedule the first process (switch from idle to init).
    schedule();
}

/* Enqueue a task on specified CPU's runqueue (insert at tail of circular list) */
void enqueue_task(struct task_struct *task, int cpu) {
    if (!task) return;
    struct runqueue *rq = &cpu_runqueue[cpu];
    if (rq->head == NULL) {
        rq->head = task;
        task->run_list.next = task;
        task->run_list.prev = task;
    } else {
        struct task_struct *head = rq->head;
        struct task_struct *tail = head->run_list.prev;
        kprintf("ch1");
        /* insert task between tail and head */
        tail->run_list.next = task;
        kprintf("ch2");
        task->run_list.prev = tail;
        task->run_list.next = head;
        head->run_list.prev = task;
    }
}

/* Remove a task from runqueue (if present). Assumes task is not running or already removed. */
void dequeue_task(struct task_struct *task, int cpu) {
    struct runqueue *rq = &cpu_runqueue[cpu];
    if (!task || rq->head == NULL) return;
    if (task->run_list.next == task) {
        /* Only task in queue */
        rq->head = NULL;
    } else {
        /* Splice out */
        struct task_struct *prev = task->run_list.prev;
        struct task_struct *next = task->run_list.next;
        prev->run_list.next = next;
        next->run_list.prev = prev;
        if (rq->head == task) {
            rq->head = next;
        }
    }
    task->run_list.next = task->run_list.prev = NULL;
}

/* Core scheduling routine called from timer interrupt context.
 * 'context' is a pointer to the saved CPU context (register state) of the task that was running.
 */
void *schedule_irq(void *context) {
    int cpu = get_cpu_id();
    rq_lock(cpu);

    struct runqueue *rq = &cpu_runqueue[cpu];
    struct task_struct *current = rq->curr;
    /* Save current task's context pointer */
    if (current) {
        current->context = context;
        if (current->state == TASK_RUNNING) {
            current->state = TASK_READY;  // put back as ready
        }
    }

    /* Pick next task to run */
    struct task_struct *next = NULL;
    if (rq->head == NULL) {
        next = current;  // no other tasks
    } else {
        /* Simple round-robin: find the first task after current that is ready */
        if (current == NULL || current->run_list.next == NULL) {
            next = rq->head;
        } else {
            next = current->run_list.next;
        }
        /* Skip tasks that are not READY */
        struct task_struct *start = next;
        while (next->state != TASK_READY) {
            next = next->run_list.next;
            if (next == start) break;
        }
        if (next->state != TASK_READY) {
            /* If none ready (all blocked), run idle */
            next = rq->head; 
            while (next && next->pid != 0 && next->run_list.next != rq->head) {
                next = next->run_list.next;
            }
        }
    }
    next->state = TASK_RUNNING;
    rq->curr = next;
    current_task = next;
    rq_unlock(cpu);
    /* Return pointer to next task's saved context (for assembly to load) */
    return next->context;
}

void schedule(void) {
    int cpu = get_cpu_id();
    rq_lock(cpu);
    struct task_struct *current = cpu_runqueue[cpu].curr;
    if (!current) {
        rq_unlock(cpu);
        return;
    }
    // Save current state if running in kernel mode
    if (current->state == TASK_RUNNING) {
        current->state = TASK_READY;
    }
    struct task_struct *next = current;
    struct runqueue *rq = &cpu_runqueue[cpu];
    do {
        next = next->run_list.next ? next->run_list.next : rq->head;
        if (!next) {
            next = current;
            break;
        }
    } while (next->state != TASK_READY && next != current);
    if (next->state != TASK_READY) {
        next = current;
    }
    next->state = TASK_RUNNING;
    rq->curr = next;
    current_task = next;
    rq_unlock(cpu);

    if (next != current) {
        if (next->user_mode) {
            // For a user process, perform a full state restore via iretq.
            // Pass the new CR3 value from the process's address space.
            kprintf("switching");
            struct cpu_context *frame = (struct cpu_context *)next->context;
            frame->rflags = 0x202;
            kprintf("RIP: %llx\n", frame->rip);
            kprintf("CS: %llx\n", frame->cs);
            kprintf("RFLAGS: %llx\n", frame->rflags);
            kprintf("RSP: %llx\n", frame->rsp);
            kprintf("SS: %llx\n", frame->ss);
            kprintf("CR3: %llx\n", next->mm->page_table);
            uint64_t lstar;
            __asm__ volatile ("rdmsr" : "=A"(lstar) : "c"(0xC0000082));
            kprintf("IA32_LSTAR = %p\n", (void*)lstar);   // should print syscall_entry’s address

            set_kernel_rsp0(next->kernel_stack_top);

            switch_to_user(next->context, (uint64_t)next->mm->page_table);
            // Note: switch_to_user does not return.
        } else {
            switch_to(current, next);
        }
    }
}
