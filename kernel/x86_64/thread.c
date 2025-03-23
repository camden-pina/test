/*
 * Kernel Thread & Process Management (Freestanding Version)
 *
 * This implementation is designed for a freestanding environment and avoids reliance on
 * the standard C library. It includes:
 *   - A basic bump allocator (kmalloc/kfree).
 *   - A custom context switching routine (using inline assembly) for x86_64.
 *   - Thread creation via a startup stub (thread_stub) that retrieves startup parameters.
 *   - Cooperative scheduling with yield and sleep functions.
 *   - Minimal kernel-style system calls (sys_brk, sys_set_tid_address, sys_exit_group).
 *   - A simple spinlock interface.
 *
 * Note: In a production kernel, you would replace these stubs with hardware-
 * specific implementations.
 */

 #include <thread.h>
 #include <stdarg.h>
 #include <stddef.h>
 #include <stdint.h>
 #include <mm/pmm.h>
 #include <panic.h>
 
 /* -------------------- Global Variables -------------------- */
 static thread_t *thread_list = NULL;   // Circular ready queue.
 static thread_t *current_thread = NULL; // Currently running thread.
 static proc_t *proc0 = NULL;            // Initial process.
 static int next_tid = 1;
 static int next_pid = 1;
 volatile uint64_t kernel_ticks = 0;     // Global tick counter.
 
 extern void context_switch(context_t *old, context_t *new);
 
 /* -------------------- Thread Startup Stub -------------------- */
 /* This function is the entry point for new threads. It pops the startup parameter
    from the thread’s stack and then calls the actual thread function. */
 __attribute__((noreturn)) void thread_stub(void) {
     uintptr_t startup_ptr;
     __asm__ __volatile__("pop %0" : "=r"(startup_ptr));
     struct thread_startup *startup = (struct thread_startup *)startup_ptr;
     if (!startup) {
         kprintf("thread_stub: startup pointer is NULL\n");
         while(1);
     }
     startup->func(startup->arg);
     kprintf("thread_stub: thread function returned, calling thread_exit\n");
     thread_exit();
     while(1);
 }
 
 /* -------------------- Thread Management -------------------- */
 
 /* Allocate a new thread and its stack. */
 static thread_t *thread_alloc(uint32_t flags, size_t stack_size) {
     thread_t *td = kmalloc(sizeof(thread_t));
     if (!td)
         panic("thread_alloc: no memory for thread descriptor");
     /* Zero out the structure (simple loop instead of memset). */
     for (size_t i = 0; i < sizeof(thread_t); i++)
         ((char*)td)[i] = 0;
     td->stack = kmalloc(stack_size);
     if (!td->stack)
         panic("thread_alloc: no memory for thread stack");
     td->flags = flags;
     td->pri_base = 0;
     td->priority = 0;
     td->cpu_id = 0;
     td->tid = next_tid++;
     td->tidptr = 0;
     td->state = THREAD_READY;
     td->sleep_until = 0;
     return td;
 }
 
 /* Allocate the main thread (proc0's thread). Uses the existing (boot) stack. */
 static thread_t *thread_alloc_proc0_main(void) {
     thread_t *td = kmalloc(sizeof(thread_t));
     if (!td)
         panic("thread_alloc_proc0_main: no memory for thread descriptor");
     for (size_t i = 0; i < sizeof(thread_t); i++)
         ((char*)td)[i] = 0;
     td->stack = NULL; /* Uses current boot stack. */
     td->flags = 0;
     td->pri_base = 0;
     td->priority = 0;
     td->cpu_id = 0;
     td->tid = next_tid++;
     td->tidptr = 0;
     td->state = THREAD_RUNNING;
     td->sleep_until = 0;
     return td;
 }
 
 /* Create a new kernel thread. Sets up the thread’s stack so that when scheduled,
    it will call thread_stub() which then invokes the desired function. */
 void create_kernel_thread(thread_func_t func, void *arg, const char *name) {
     (void)name; // Name unused in this simulation.
     kprintf("create_kernel_thread: Creating thread\n");
     thread_t *td = thread_alloc(0x1 /*TDF_KTHREAD*/, STACK_SIZE);
     struct thread_startup *startup = kmalloc(sizeof(struct thread_startup));
     if (!startup)
         panic("create_kernel_thread: no memory for startup structure");
     startup->func = func;
     startup->arg  = arg;
     /* Prepare the new thread’s stack.
        Place the startup pointer on the stack so that thread_stub() can pop it. */
     uint64_t *stack_top = (uint64_t *)((uint8_t *)td->stack + STACK_SIZE);
     *(--stack_top) = (uint64_t)startup;  // Push startup pointer.
     td->context.rsp = (uint64_t)stack_top;
     td->context.rip = (uint64_t) thread_stub;
     /* Insert the new thread into the circular ready queue. */
     if (!thread_list) {
         thread_list = td;
         td->next = td;
     } else {
         td->next = thread_list->next;
         thread_list->next = td;
     }
     kprintf("create_kernel_thread: Thread created with TID %d\n", td->tid);
 }
 
 /* Yield the processor: switch to the next ready thread. */
 void thread_yield(void) {
     if (!current_thread)
         return;
     if (current_thread->next == current_thread)
         return;  // Only one runnable thread.
     thread_t *prev = current_thread;
     if (prev->state == THREAD_RUNNING)
         prev->state = THREAD_READY;
     current_thread = current_thread->next;
     current_thread->state = THREAD_RUNNING;
     context_switch(&prev->context, &current_thread->context);
 }
 
 /* Terminate the current thread. Frees resources and switches context. */
 void thread_exit(void) {
     kprintf("thread_exit: Terminating thread TID %d\n", current_thread->tid);
     current_thread->state = THREAD_TERMINATED;
     if (current_thread->next == current_thread) {
         kprintf("thread_exit: Last thread; halting system.\n");
         while (1);
     }
     thread_t *prev = current_thread;
     while (prev->next != current_thread)
         prev = prev->next;
     prev->next = current_thread->next;
     thread_t *next_thread = current_thread->next;
     if (current_thread->stack)
         kfree(current_thread->stack);
     kfree(current_thread);
     current_thread = next_thread;
     current_thread->state = THREAD_RUNNING;
     context_switch(&prev->context, &current_thread->context);
     while (1);
 }
 
 /* Put the current thread to sleep for a given number of ticks. */
 void thread_sleep(uint64_t ticks) {
     current_thread->sleep_until = kernel_ticks + ticks;
     current_thread->state = THREAD_SLEEPING;
     thread_yield();
 }
 
 /* (Optional) Wake a specific thread by setting its state to READY. */
 void thread_wakeup(thread_t *thread) {
     if (thread->state == THREAD_SLEEPING)
         thread->state = THREAD_READY;
 }
 
 /* -------------------- Process and Scheduler -------------------- */
 
 /* Initialize the initial process (proc0) and its main thread. */
 void proc0_init(void) {
     kprintf("proc0_init: Initializing proc0\n");
     proc0 = kmalloc(sizeof(proc_t));
     if (!proc0)
         panic("proc0_init: no memory for proc0");
     for (size_t i = 0; i < sizeof(proc_t); i++)
         ((char*)proc0)[i] = 0;
     proc0->pid = 0;
     proc0->heap_base = kmalloc(HEAP_SIZE);
     if (!proc0->heap_base)
         panic("proc0_init: no memory for process heap");
     proc0->heap_capacity = HEAP_SIZE;
     proc0->brk = proc0->heap_base;
     thread_t *main_td = thread_alloc_proc0_main();
     proc0->main_thread = main_td;
     current_thread = main_td;
     thread_list = main_td;
     main_td->next = main_td;
     kprintf("proc0_init: proc0 (PID %d) and main thread (TID %d) initialized\n",
             proc0->pid, main_td->tid);
 }
 
 /* Initialize the scheduler (for now, just proc0). */
 void sched_init(void) {
     proc0_init();
 }
 
 /* Run the scheduler: in this cooperative model, repeatedly yield. */
 void sched_run(void) {
     while (1) {
         thread_yield();
     }
 }
 
 /* Simulated timer tick handler. In a real kernel, this would be called by the timer interrupt. */
 void timer_tick(void) {
     kernel_ticks++;
     if (!thread_list)
         return;
     thread_t *iter = thread_list;
     do {
         if (iter->state == THREAD_SLEEPING && iter->sleep_until <= kernel_ticks) {
             iter->state = THREAD_READY;
         }
         iter = iter->next;
     } while (iter != thread_list);
 }
 
 /* -------------------- Syscall / Kernel Service Implementations -------------------- */
 
 unsigned long sys_brk(unsigned long addr) {
     if (!proc0)
         return 0;
     if (addr == 0)
         return (unsigned long) proc0->brk;
     if (addr < (unsigned long) proc0->heap_base ||
         addr > (unsigned long) proc0->heap_base + proc0->heap_capacity)
     {
         return (unsigned long) proc0->brk;
     }
     proc0->brk = (char *) addr;
     return addr;
 }
 
 long sys_set_tid_address(const int *tidptr) {
     current_thread->tidptr = tidptr;
     return current_thread->tid;
 }
 
 void sys_exit_group(int error_code) {
     (void) error_code;
     kprintf("sys_exit_group: Halting system.\n");
     while(1);
 }
 
 /* -------------------- Test Thread Functions -------------------- */
 void test_thread(void *arg) {
     (void)arg;
     kprintf("test_thread: Running on TID %d\n", current_thread->tid);
     for (volatile int i = 0; i < 1000000; i++) {
         ; // Simulate work.
     }
     kprintf("test_thread: Sleeping for 5 ticks\n");
     thread_sleep(5);
     kprintf("test_thread: Woke up, exiting thread\n");
     thread_exit();
 }
 
 void idle_thread(void *arg) {
     (void)arg;
     while (1) {
         kprintf("idle_thread: TID %d idle\n", current_thread->tid);
         thread_yield();
     }
 }
 
 /* -------------------- Spinlock Implementation -------------------- */
 void spinlock_init(spinlock_t *lock) {
     *lock = SPINLOCK_UNLOCKED;
 }
 
 void spinlock_lock(spinlock_t *lock) {
     while (__sync_lock_test_and_set(lock, SPINLOCK_LOCKED) == SPINLOCK_LOCKED)
         ;
 }
 
 void spinlock_unlock(spinlock_t *lock) {
     __sync_lock_release(lock);
 }
 