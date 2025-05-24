
#include "task.h"
#include "sched.h"
#include "syscall.h"
// #include "elf.h"
#include "context.h"
#include <stddef.h>
#include <string.h>  // for memset
#include <mm/pgtable.h>
#include <mm/vmem.h>
#include <file/elf_loader.h>
#include <mm/pmm.h>

int next_pid = 1;
struct task_struct *current_task = NULL;

void thread_exit(void);

/* Allocate and initialize a new task_struct */
struct task_struct *alloc_task(void) {
    struct task_struct *t = (struct task_struct *)kmalloc(sizeof(struct task_struct));
    if (!t) return NULL;
    memset(t, 0, sizeof(struct task_struct));
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->cpu = 0; // get_cpu_id();
    t->parent = NULL;
    t->run_list.prev = t->run_list.next = NULL;
    t->children.prev = t->children.next = NULL;
    t->sibling.prev = t->sibling.next = NULL;
    t->context = NULL;
    t->kernel_stack_size = 0;
    t->kernel_stack = NULL;
    t->kernel_stack_top = 0;
    // Note: t->mm is now expected to point to an address_space_t.
    t->mm = NULL;
    t->files = NULL;
    t->exit_code = 0;
    t->exited = false;
    t->waiter = NULL;
    return t;
}

void init_task_context(struct task_struct *task, void *start_ip, void *start_sp, bool user_mode) {
    // Allocate a kernel stack for the task
    task->kernel_stack_size = 16384;  // 16 KB
    task->kernel_stack = (uint8_t*)kmalloc(task->kernel_stack_size);
    memset(task->kernel_stack, 0, task->kernel_stack_size);
    task->kernel_stack_top = (uint64_t)task->kernel_stack + task->kernel_stack_size;

    if (user_mode) {
        // Set up an interrupt frame for iretq to user mode
        struct cpu_context *frame = (struct cpu_context*)(task->kernel_stack_top - sizeof(struct cpu_context));
        memset(frame, 0, sizeof(struct cpu_context));
        frame->rip = (uint64_t)start_ip;
        frame->cs = 0x2B;         // User mode code segment (RPL 3)
        frame->rflags = 0x202;    // Interrupt flag enabled
        frame->rsp = (uint64_t)start_sp;
        frame->ss = 0x23;         // User mode data segment (RPL 3)
        task->context = frame;    // Save pointer to the interrupt frame
    } else {
        // For kernel threads, prepare a simple context that uses 'ret'
        uint64_t *stack_top = (uint64_t*)task->kernel_stack_top;
        *(--stack_top) = (uint64_t)thread_exit;  // fake return address
        task->context = stack_top;
    }
}

/* Create an idle task (kernel thread) for a given CPU */
struct task_struct *create_idle_task(int cpu) {
    struct task_struct *idle = alloc_task();
    if (!idle) return NULL;
    idle->pid = 0;
    idle->cpu = cpu;
    idle->parent = NULL;
    idle->state = TASK_READY;
    /* Idle loop function (defined below) */
    extern void idle_loop(void);
    init_task_context(idle, idle_loop, NULL, false);
    return idle;
}

/* Idle loop function - runs with interrupts enabled to allow preemption */
void idle_loop(void) {
    for (;;) {
        __asm__ __volatile__("sti\n hlt\n");  /* Halt CPU until next interrupt (saves power) */
    }
}

/* Create a new kernel thread */
struct task_struct *create_kernel_thread(void (*start_routine)(void *), void *arg) {
    struct task_struct *t = alloc_task();
    if (!t) return NULL;
    t->parent = current_task;
    // Kernel threads share the address space of the current task.
    t->mm = current_task ? current_task->mm : NULL;
    t->files = current_task ? current_task->files : NULL;
    /* Initialize context: kernel thread starting at start_routine */
    init_task_context(t, (void*)start_routine, NULL, false);
    // Set up argument for the thread (e.g., via saved register or stack)
    // We can store arg in a known register location on the new stack if needed.
    return t;
}

struct task_struct *create_user_process(const char *path, char *const argv[], char *const envp[]) {
    struct task_struct *t = alloc_task();
    if (!t) return NULL;
    t->parent = NULL;
    t->user_mode = true; // Mark as a user mode process

    // Allocate a new address space and load the ELF binary
    t->mm = (address_space_t*)kmalloc(sizeof(address_space_t));
    memset(t->mm, 0, sizeof(address_space_t));
    t->mm->page_table = allocate_table_page(); // allocate a new PML4
    t->mm->page_table = clone_kernel_space((uint64_t*)t->mm->page_table);

    uint64_t entry = 0;
    uint64_t user_stack_top = 0;
    if (load_elf_binary(path, t->mm, &entry, &user_stack_top) < 0) {
        kprintf("Failed to load ELF for %s\n", path);
        kfree((void*)t->mm->page_table);
        kfree(t->mm);
        kfree(t);
        return NULL;
    }
    init_task_context(t, (void*)entry, (void*)user_stack_top, true);

    // Initialize file table (stub)
    t->files = (struct files_struct*)kmalloc(sizeof(struct files_struct));
    memset(t->files, 0, sizeof(struct files_struct));
    return t;
}

/* Terminate current task with exit_code */
void task_exit(int exit_code) {
    struct task_struct *curr = current_task;
    curr->exit_code = exit_code;
    curr->exited = true;
    curr->state = TASK_ZOMBIE;
    /* Clean up address space if any */
    if (curr->mm && curr->mm != NULL) {
        // Free user memory (stubbed: not actually freeing all pages)
        kfree((void*)curr->mm->page_table);
        kfree(curr->mm);
        curr->mm = NULL;
    }
    /* Wake up any waiter (e.g., parent) */
    if (curr->waiter) {
        curr->waiter->state = TASK_READY;
    }
    /* Remove from runqueue; this will be done in schedule as well. */
    int cpu = get_cpu_id();
    rq_lock(cpu);
    dequeue_task(curr, cpu);
    rq_unlock(cpu);
    /* Switch to next task (will not return) */
    schedule();
    /* We should never reach here */
    kprintf("Error: task_exit returned for pid %d\n", curr->pid);
    for(;;) { __asm__("hlt"); }
}

/* Wait for a child process to exit (simplified waitpid) */
struct task_struct *task_wait(int pid, int *status) {
    struct task_struct *curr = current_task;
    /* Search for a child with matching pid */
    struct task_struct *child = curr->children.next;
    while (child && child != (struct task_struct*)&curr->children) {
        if (child->pid == pid || pid == -1) {
            /* Found a child (or any child if pid == -1) */
            if (!child->exited) {
                /* Not exited yet - block current */
                curr->state = TASK_BLOCKED;
                child->waiter = curr;
                schedule();  // switch out until child exits
            }
            /* Child exited */
            if (status) *status = child->exit_code;
            // Remove child from children list
            if (child->sibling.prev)
                child->sibling.prev->sibling.next = child->sibling.next;
            if (child->sibling.next)
                child->sibling.next->sibling.prev = child->sibling.prev;
            struct task_struct *res = child;
            kfree(child->kernel_stack);
            kfree(child);
            return res;
        }
        child = child->sibling.next;
    }
    return NULL;  // no such child
}

/* A simple thread_exit function for kernel threads to call when finishing. */
void thread_exit(void) {
    task_exit(0);
}