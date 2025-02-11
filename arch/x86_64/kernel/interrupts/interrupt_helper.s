.macro save_context
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax
.endm

.macro restore_context
    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
#    movdqu xmm15, (rsp)
#    movdqu xmm14, (rsp+16)
#    movdqu xmm13, (rsp+32)
#    movdqu xmm12, (rsp+48)
#    movdqu xmm11, (rsp+64)
#    movdqu xmm10, (rsp+80)
#    movdqu xmm9, (rsp+96)
#    movdqu xmm8, (rsp+112)
#    movdqu xmm7, (rsp+128)
#    movdqu xmm6, (rsp+144)
#    movdqu xmm5, (rsp+160)
#    movdqu xmm4, (rsp+176)
#    movdqu xmm3, (rsp+192)
#    movdqu xmm2, (rsp+208)
#    movdqu xmm1, (rsp+224)
#    movdqu xmm0, (rsp+240)
.endm

.extern isr_common

.macro isr_noerror num
.globl isr\num
isr\num:
    save_context
    
    movq $0, %rsi
    movq $\num, %rdi
    call isr_common
    
    restore_context
    
    iretq
.endm

.macro isr_error num
.globl isr\num
isr\num:
    save_context
    
    movq 128(%rsp), %rsi
    movq $\num, %rdi
    call isr_common
    
    restore_context
    
    iretq
.endm

isr_noerror 0
isr_noerror 1
isr_noerror 2
isr_noerror 3
isr_noerror 4
isr_noerror 5
isr_error 6    # doesnt give an error code; may change later
isr_noerror 7
isr_error 8
isr_noerror 9
isr_error 10
isr_error 11
isr_error 12
isr_error 13
isr_error 14
isr_noerror 16
isr_error 17
isr_noerror 18
isr_noerror 19
isr_noerror 20
# isr 20-29 are reserved
isr_error 30	# was 'isr_noerror'; table on OsDev says this provides an error code
#isr 31 is reserved
isr_noerror 32
isr_noerror 33
isr_noerror 34 
isr_noerror 35
isr_noerror 36
isr_noerror 37
isr_noerror 38
isr_noerror 39
isr_noerror 40
isr_noerror 41
isr_noerror 42
isr_noerror 43
isr_noerror 44
isr_noerror 45
isr_noerror 46
isr_noerror 47
isr_noerror 128
