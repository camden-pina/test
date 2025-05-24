#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdint.h>

/*
 * CPU Context structure for x86_64.
 *
 * This structure is used to save the state of a task's CPU registers during
 * context switching. It includes all general-purpose registers as well as the
 * instruction pointer (RIP), code segment (CS), flags (RFLAGS), stack pointer (RSP),
 * and stack segment (SS) required for returning to either user mode (via iretq/sysret)
 * or to resume kernel execution.
 *
 * Note: The order and layout of these fields is critical and must match the
 * expectations of the assembly context switch routines.
 */
struct cpu_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    /* The following fields are used for returning via iretq/sysret to user space */
    uint64_t rip;     /* Instruction Pointer */
    uint64_t cs;      /* Code Segment */
    uint64_t rflags;  /* RFLAGS register */
    uint64_t rsp;     /* Stack Pointer */
    uint64_t ss;      /* Stack Segment */
} __attribute__((packed));


#ifdef __cplusplus
extern "C" {
#endif

/**
 * syscall_entry
 *
 * This is the entry point for system calls from user space. It is implemented in assembly
 * (in context.S) and is set as the target of the IA32_LSTAR MSR.
 *
 * On entry, it performs necessary privilege and context switches from user mode to kernel mode.
 */
void syscall_entry(void);

/**
 * switch_to
 *
 * Perform a context switch from the current task to the next task.
 * This function is implemented in assembly and does not return to the caller.
 *
 * @param prev: Pointer to the current task_struct (whose context will be saved)
 * @param next: Pointer to the task_struct of the task to switch to (whose context will be restored)
 */
void switch_to(struct task_struct *prev, struct task_struct *next);

/**
 * @brief Switch to a user-mode process using an interrupt frame.
 *
 * This function restores the full CPU state for a user process from a pointer to a
 * pre-populated interrupt frame. The expected memory layout of the frame (in order) is:
 * - Offset 0: Instruction pointer (RIP)
 * - Offset 8: Code segment (CS) (typically 0x1B for user mode)
 * - Offset 16: RFLAGS (with interrupts enabled, e.g. 0x202)
 * - Offset 24: Stack pointer (RSP)
 * - Offset 32: Stack segment (SS) (typically 0x23 for user mode)
 *
 * It pushes these values onto the stack and executes an iretq to perform the transition
 * to user mode. This function does not return.
 *
 * @param user_context Pointer to the CPU context (interrupt frame) for the user process.
 */
__attribute__((noreturn))
void switch_to_user(void *user_context, uint64_t new_cr3);

#ifdef __cplusplus
}
#endif

#endif /* CONTEXT_H */
