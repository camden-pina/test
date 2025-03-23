    .global switch_context
    .type switch_context, @function
/*
 * void switch_context(unsigned long **old_stack, unsigned long *new_stack)
 *
 * Saves the current %rsp into *old_stack, then loads new_stack into %rsp,
 * effectively switching contexts.
 */
switch_context:
    movq %rsp, (%rdi)   /* Save current stack pointer into *old_stack */
    movq %rsi, %rsp     /* Load new_stack into rsp */
    ret

	.text
	.globl context_switch
	.type context_switch, @function
context_switch:
    # Save callee-saved registers into the old context (first argument in RDI)
    movq %rbx, 0(%rdi)
    movq %rbp, 8(%rdi)
    movq %r12, 16(%rdi)
    movq %r13, 24(%rdi)
    movq %r14, 32(%rdi)
    movq %r15, 40(%rdi)
    movq %rsp, 48(%rdi)
    leaq 1f(%rip), %rax
    movq %rax, 56(%rdi)
    
    # Load new context from second argument in RSI
    movq 0(%rsi), %rbx
    movq 8(%rsi), %rbp
    movq 16(%rsi), %r12
    movq 24(%rsi), %r13
    movq 32(%rsi), %r14
    movq 40(%rsi), %r15
    movq 48(%rsi), %rsp
    movq 56(%rsi), %rax
    jmp *%rax
1:
    ret
