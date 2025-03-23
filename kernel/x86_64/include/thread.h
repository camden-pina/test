#ifndef KERNEL_THREAD_MANAGER_H
#define KERNEL_THREAD_MANAGER_H

#include <stdint.h>
#include <stddef.h>

/* -------------------- Configuration -------------------- */

#define STACK_SIZE         (64 * 1024)      // 64 KB per thread stack
#define HEAP_SIZE          (1024 * 1024)    // 1 MB process heap
#define KERNEL_HEAP_SIZE   (2 * 1024 * 1024) // 2 MB kernel heap (for kmalloc)

/* -------------------- Data Structures -------------------- */

/* Thread states */
typedef enum thread_state {
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_BLOCKED,
    THREAD_SLEEPING,
    THREAD_TERMINATED
} thread_state_t;

/* CPU context structure for x86_64. This saves callee‐saved registers plus RSP and RIP. */
typedef struct context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rip;
} context_t;

/* Function pointer type for thread functions. */
typedef void (*thread_func_t)(void *);

/* Structure to pass startup information to a new thread. */
struct thread_startup {
    thread_func_t func;
    void *arg;
};

/* Thread descriptor. */
typedef struct thread {
    context_t context;      // Saved CPU context.
    void *stack;            // Pointer to allocated stack (NULL for main thread).
    struct thread *next;    // Next thread in the circular ready queue.
    uint32_t flags;         // Thread flag bits (e.g., kernel thread, idle, etc.).
    uint8_t pri_base;       // Base priority (for future use).
    uint8_t priority;       // Current priority (for future use).
    int cpu_id;             // CPU id (for simulation; single-CPU here).
    int tid;                // Thread ID.
    const int *tidptr;      // For sys_set_tid_address.
    thread_state_t state;   // Current state.
    uint64_t sleep_until;   // Tick count until which the thread sleeps.
} thread_t;

/* Process descriptor. */
typedef struct proc {
    int pid;                // Process ID.
    thread_t *main_thread;  // The main thread (the one running at boot).
    char *heap_base;        // Base address of the process heap.
    size_t heap_capacity;   // Process heap capacity.
    char *brk;              // Current break pointer (end of allocated heap).
} proc_t;

/* -------------------- Thread Management -------------------- */
void proc0_init(void);
void create_kernel_thread(thread_func_t func, void *arg, const char *name);
void thread_yield(void);
void thread_exit(void);
void thread_sleep(uint64_t ticks);
void thread_wakeup(thread_t *thread);  // Optional: for waking a specific thread.

/* -------------------- Syscall / Kernel Services -------------------- */
unsigned long sys_brk(unsigned long addr);
long sys_set_tid_address(const int *tidptr);
void sys_exit_group(int error_code);

/* -------------------- Scheduler and Timer -------------------- */
void sched_init(void);
void sched_run(void);
void timer_tick(void);  // Called by a (simulated) timer interrupt.
extern volatile uint64_t kernel_ticks; // Global tick counter.

/* -------------------- Test Thread Functions -------------------- */
void test_thread(void *arg);
void idle_thread(void *arg);

/* -------------------- Spinlock -------------------- */
typedef volatile int spinlock_t;
#define SPINLOCK_UNLOCKED 0
#define SPINLOCK_LOCKED   1

void spinlock_init(spinlock_t *lock);
void spinlock_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);

#endif // KERNEL_THREAD_MANAGER_H
