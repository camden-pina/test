#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "syscall.h"  /* for file descriptor table stub */
#include <mm_types.h>

/* Forward declaration for mm and files structs (simplified) */
struct mm_struct;
struct files_struct;

/* Doubly-linked list node for runqueue */
struct task_list {
    struct task_struct *prev;
    struct task_struct *next;
};

/* Task control block (PCB) structure */
struct task_struct {
    int             pid;
    int             state;
    int             cpu;               /* CPU index the task is assigned to */
    struct task_struct *parent;
    struct task_list   children;       /* List head for child tasks */
    struct task_list   sibling;        /* Node in parent's children list */
    struct task_list   run_list;       /* Node in runqueue list */

    /* Scheduling and context switching */
    void          *context;           /* Saved context (stack pointer) for switching */
    uint8_t       *kernel_stack;      /* Base of kernel stack memory */
    uint64_t       kernel_stack_size;
    uint64_t       kernel_stack_top;  /* Top of kernel stack (stack pointer initial value) */

    /* Memory space and resources */
    struct address_space    *mm;          /* Address space (memory map) */
    struct files_struct *files;       /* Open files (stubbed) */

    /* Exit status and zombie linkage */
    int exit_code;
    bool exited;
    struct task_struct *waiter;       /* Task (usually parent) waiting for this to exit */
    bool user_mode;
};

/* Simple memory map structure (address space) */
struct mm_struct {
    uint64_t *pml4;      /* PML4 base (virtual address) */
    uint64_t  pml4_phys; /* Physical address of PML4 */
    // Additional fields like list of VMAs could be added here
};

/* File descriptor table stub */
struct files_struct {
    // Placeholder for open file descriptors
    void *dummy;
};

/* Global PID counter and task list */
extern int next_pid;
extern struct task_struct *current_task;

/* Create a new kernel thread task */
struct task_struct *create_kernel_thread(void (*start_routine)(void *), void *arg);

/* Create a new user process by loading an ELF binary */
struct task_struct *create_user_process(const char *path, char *const argv[], char *const envp[]);

/* Create idle task for a CPU */
struct task_struct *create_idle_task(int cpu);

/* Low-level task setup helpers */
void init_task_context(struct task_struct *task, void *start_ip, void *start_sp, bool user_mode);

/* Task termination handling */
void task_exit(int exit_code);
struct task_struct *task_wait(int pid, int *status);

struct task_struct *alloc_task(void);

#endif /* TASK_H */
