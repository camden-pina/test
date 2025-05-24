        .text
        .globl syscall6
        .type syscall6, @function
syscall6:
        .cfi_startproc
        /* Standard prologue: save caller’s frame pointer */
        push    %rbp
        .cfi_def_cfa_offset 16       /* CFA now RSP+16 after push */
        .cfi_offset rbp, -16         /* Saved RBP is at offset -16 */
        mov     %rsp, %rbp
        .cfi_def_cfa_register rbp

        /* 
           At this point the registers (from the caller) should be:
           RDI: syscall number (SYS_write, 5)
           RSI: a1 (should be fd, 4) 
           RDX: a2 (pointer to buf)
           RCX: a3 (3)
           R8 : a4 (0)
           R9 : a5 (0)
           And the sixth argument, a6 (0), is on the stack.
         */
        /* (For debugging, you could store RSI to a slot in the stack frame here.)
           For example: 
           mov     %rsi, -8(%rbp)   ; save to local variable (check with debugger)
         */

.halt:
        jmp .halt   /* Halt loop for inspection—do not proceed to moves yet! */

        /* === The following argument rearrangements are not reached in the halt loop === */

        /* Rearrangement for Linux syscalls:
           Move syscall number into RAX, then shift parameters as:
              a1 becomes RDI, a2 becomes RSI, a3 becomes RDX,
              a4 goes to R10, a5 goes to R8, and a6 (from the stack) goes to R9.
         */
        mov     %rdi, %rax         /* sys-call number → RAX */
        mov     %rsi, %rdi         /* a1 → RDI */
        mov     %rdx, %rsi         /* a2 → RSI */
        mov     %rcx, %rdx         /* a3 → RDX */
        mov     %r8,  %r10         /* a4 → R10 */
        mov     %r9,  %r8          /* a5 → R8 */
        mov     16(%rbp), %r9       /* a6 (from stack) → R9 */

        syscall                    /* perform the system call */

        /* Standard epilogue: restore caller’s frame pointer */
        pop     %rbp
        .cfi_restore rbp
        ret
        .cfi_endproc
        .size syscall6, .-syscall6
