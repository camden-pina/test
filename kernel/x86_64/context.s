#	.text
#	.balign 16
#	.globl switch_to
#	.type switch_to, @function

    .text
    .p2align 4, 0x90
    .global syscall_entry
    .global switch_to
    .type switch_to, @function
switch_to:
    # Save callee-saved registers (for kernel threads)
    pushq   %rbx
    pushq   %rbp
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    # Save current task's stack pointer into prev->context
    movq    %rsp, (%rdi)

    # Load next task's saved stack pointer into RSP
    movq    (%rsi), %rsp

    # Restore callee-saved registers from new stack and return
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbp
    popq    %rbx
    ret

    .p2align 4, 0x90
    .global switch_to_user
    .type switch_to_user, @function
switch_to_user:
    # rdi points to the cpu_context structure.
    # rsi holds the new CR3 value for the user process's page table.

    # Update CR3 with the new page table base.
    movq    %rsi, %cr3

    # Optional: Clear data segment registers
    xor     %rax, %rax
    mov     %rax, %ds
    mov     %rax, %es
    mov     %rax, %fs
    mov     %rax, %gs

    # Push stack frame for iretq in reverse order (SS, RSP, RFLAGS, CS, RIP)
    pushq   152(%rdi)   # Push SS (offset 152)
    pushq   144(%rdi)   # Push RSP (offset 144)
    pushq   136(%rdi)   # Push RFLAGS (offset 136)
    pushq   128(%rdi)   # Push CS (offset 128)
    pushq   120(%rdi)   # Push RIP (offset 120)

    iretq

/* ===================================================================== */
/*  x86‑64 SYSCALL fast‑path                                             */
/*                                                                       */
/*  Assumptions                                                          */
/*  ───────────                                                          */
/*    – IA32_EFER.SCE      = 1                                           */
/*    – IA32_STAR[47:32]   = kernel CS (0x08)                            */
/*      IA32_STAR[63:48]   = user  selector base (0x20)                  */
/*    – IA32_LSTAR         = &syscall_entry                              */
/*    – IA32_FMASK         = (1<<17) | (1<<9) | (1<<8)   ; RF, IF, TF    */
/*    – %gs.base (kernel) points to percpu area with → kernel_rsp0       */
/* ===================================================================== */

        .p2align 4, 0x90
        .globl  syscall_entry
syscall_entry:
        /* -------------------------------------------------------------- */
        /* 1.  Enter kernel mode                                          */
        /* -------------------------------------------------------------- */
        swapgs                          /* %gs now references percpu data */

        /* -------------------------------------------------------------- */
        /* 2.  Switch to a scratch kernel stack                           */
        /*      r11 = user RFLAGS on entry                                */
        /*      rcx = user RIP    on entry                                */
        /* -------------------------------------------------------------- */
        mov     %r11, %r13              /* save user RFLAGS temporarily   */
        mov     kernel_rsp0(%rip), %r11 /* r11 ← kernel stack pointer     */
        xchg    %rsp, %r11              /* rsp = k‑stack, r11 = user rsp  */

        /* -------------------------------------------------------------- */
        /* 3.  Build a minimal SYSRET frame (user‑mode order)             */
        /*      [rsp+00] = user RSP                                       */
        /*      [rsp+08] = user RFLAGS                                   */
        /*      [rsp+10] = user RIP                                      */
        /* -------------------------------------------------------------- */
        pushq   %r11                    /* user RSP                       */
        pushq   %r13                    /* user RFLAGS                    */
        pushq   %rcx                    /* user RIP                       */

        /* -------------------------------------------------------------- */
        /* 4.  Align stack for the C dispatcher                           */
        /*      SysV requires rsp % 16 == 8 before CALL                   */
        /* -------------------------------------------------------------- */
        sub     $8,  %rsp               /* keep 16‑byte alignment         */

        /* -------------------------------------------------------------- */
        /* 5.  Shuffle arguments to SysV order                            */
        /* -------------------------------------------------------------- */
        mov     %r8,  %r9               /* arg5 → r9                      */
        mov     %r10, %r8               /* arg4 → r8                      */
        mov     %rdx, %rcx              /* arg3 → rcx                     */
        mov     %rsi, %rdx              /* arg2 → rdx                     */
        mov     %rdi, %rsi              /* arg1 → rsi                     */
        mov     %rax, %rdi              /* nr   → rdi                     */

        /* -------------------------------------------------------------- */
        /* 6.  Call the C dispatcher (interrupts still masked)            */
        /* -------------------------------------------------------------- */
        call    syscall_dispatch        /* returns value in rax           */

        /* -------------------------------------------------------------- */
        /* 7.  Tear down the temporary area                               */
        /* -------------------------------------------------------------- */
        add     $8,  %rsp               /* undo the sub $8                */

        /* fetch saved user frame into scratch registers                  */
        popq    %r10                    /* r10 ← user RIP                 */
        popq    %r11                    /* r11 ← user RFLAGS              */
        popq    %r12                    /* r12 ← user RSP                 */

        /* -------------------------------------------------------------- */
        /* 8.  Final return sequence — NO MORE KERNEL MEMORY ACCESSES!    */
        /* -------------------------------------------------------------- */
        mov     %r10, %rcx              /* RCX = user RIP (64‑bit move)   */
        /* Optionally: AND %r11, ~(1<<17|1<<9|1<<8)  ; mask reserved bits */

        swapgs                          /* restore user GS.base           */
        mov     %r12, %rsp              /* switch to user stack           */
        sysretq                         /* RCX→RIP, R11→RFLAGS, CPL→3     */

        /* never reached */
        .size   syscall_entry, .-syscall_entry
