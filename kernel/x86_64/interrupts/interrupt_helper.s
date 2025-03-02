    .section .text
    .globl isr_common_stub
    .type isr_common_stub,@function
isr_common_stub:
    # Save registers that might be clobbered
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # At this point, the stack layout is as follows:
    #   [vector number] (pushed by our ISR stub macro)
    #   [error code]    (either pushed as a dummy or automatically by CPU)
    # then the saved registers (15*8 = 120 bytes).
    # We load the two arguments for isr_common:
    #   Offset for vector: 120 bytes (saved regs) + 8 bytes = 128 bytes from %rsp
    #   Offset for error code: 120 bytes from %rsp
    movq 120(%rsp), %rdi    # First argument: vector number
    movl 128(%rsp), %esi    # Second argument: error code (32-bit)
    call isr_common

    # Restore registers (in reverse order)
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    # Clean up the stack: remove the vector and error code pushed by the stub
    addq $16, %rsp

    # Return from interrupt
    iretq

# Original macros for generating ISR stubs
.macro ISR_NOERR num
    .globl isr\num
    .type isr\num,@function
isr\num:
    cli                   # Disable interrupts
    pushq $0              # Push a dummy error code
    pushq $\num          # Push the interrupt vector number
    jmp isr_common_stub
.endm

.macro ISR_ERR num
    .globl isr\num
    .type isr\num,@function
isr\num:
    cli                   # Disable interrupts
    pushq $\num          # Push the interrupt vector number (error code already pushed)
    jmp isr_common_stub
.endm

# Generate ISRs for CPU exceptions (vectors 0–31)
ISR_NOERR 0    # Divide Error
ISR_NOERR 1    # Debug
ISR_NOERR 2    # Non-Maskable Interrupt (NMI)
ISR_NOERR 3    # Breakpoint
ISR_NOERR 4    # Overflow
ISR_NOERR 5    # Bound Range Exceeded
ISR_NOERR 6    # Invalid Opcode
ISR_NOERR 7    # Device Not Available
ISR_ERR 8      # Double Fault
ISR_NOERR 9    # Coprocessor Segment Overrun (reserved)
ISR_ERR 10     # Invalid TSS
ISR_ERR 11     # Segment Not Present
ISR_ERR 12     # Stack-Segment Fault
ISR_ERR 13     # General Protection Fault
ISR_ERR 14     # Page Fault
ISR_NOERR 15   # Reserved
ISR_NOERR 16   # x87 Floating-Point Exception
ISR_ERR 17     # Alignment Check
ISR_NOERR 18   # Machine Check
ISR_NOERR 19   # SIMD Floating-Point Exception
ISR_NOERR 20   # Virtualization Exception
ISR_NOERR 21   # Control Protection Exception
ISR_NOERR 22   # Reserved
ISR_NOERR 23   # Hypervisor Injection Exception
ISR_NOERR 24   # VMM Communication Exception
ISR_NOERR 25   # Security Exception
ISR_NOERR 26   # Reserved
ISR_NOERR 27   # Reserved
ISR_NOERR 28   # Reserved
ISR_NOERR 29   # Reserved
ISR_NOERR 30   # Reserved
ISR_NOERR 31   # Reserved

# Generate ISRs for hardware IRQs (vectors 32–47)
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

# System call interrupt (vector 128)
ISR_NOERR 128

# Spurious Interrupt Handler remains unchanged
.globl isr_spurious
.type isr_spurious,@function
isr_spurious:
    cli
    pushq $0
    pushq $255
    jmp isr_common_stub

# ----------------------------
# UHCI-specific ISR Wrapper
# ----------------------------

    .section .text
    .globl uhci_isr_common_stub
    .type uhci_isr_common_stub,@function
uhci_isr_common_stub:
    # Save registers (similar to isr_common_stub)
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Load arguments for UHCI-specific handler:
    #   The vector number is at offset 120 + 8 = 128, and the error code is at 120.
    movq 128(%rsp), %rdi    # First argument: vector number
    movl 120(%rsp), %esi    # Second argument: error code
    call uhci_interrupt_handler_main

    # Restore registers in reverse order
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    # Clean up the stack: remove vector number and error code (16 bytes)
    addq $16, %rsp

    # Return from interrupt
    iretq

# Macro for UHCI ISRs (no error code)
.macro UHCI_ISR_NOERR num
    .globl uhci_isr\num
    .type uhci_isr\num,@function
uhci_isr\num:
    cli                   # Disable interrupts
    pushq $0              # Push dummy error code
    pushq $\num           # Push interrupt vector number
    jmp uhci_isr_common_stub
.endm

# Example: Generate UHCI ISR for vector 0x50 (80 decimal)
UHCI_ISR_NOERR 80
