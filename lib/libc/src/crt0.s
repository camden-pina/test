    .global _start
_start:
    /* the loader should give us a stack; if not, set one up here   */
    call main
    mov  $1, %rdi      /* SYS_EXIT */
    xor  %rsi, %rsi    /* status = 0 */
    syscall
.hang: hlt; jmp .hang

