/* Use AT&T syntax by default in GAS. */
.text

/* Tell the assembler we have an external symbol named GDTR. */
.extern GDTR

.globl gdt_flush
gdt_flush:
    /* Use RIP-relative for lgdt. This ensures a 64-bit-capable relocation. */
    lgdt GDTR(%rip)

    movw $0x40, %ax
    ltr %ax

    movq %rsp, %rax
    pushq $0x10          /* New SS at offset 0x10 in the GDT */
    pushq %rax           /* New RSP */
    pushfq               /* Push RFLAGS */
    pushq $0x08          /* New CS at offset 0x08 in the GDT */

    /* Load the address of label 1 into RAX as a 64-bit value.
       Alternatively, you can do 'leaq 1f(%rip), %rax'. */
    leaq 1f(%rip), %rax
    pushq %rax

    /* iretq will pop CS:RIP from the stack (among others), effectively jumping to label 1. */
    iretq

1:
    ret

.globl load_pml4
load_pml4:
    /* movabs ensures a full 64-bit immediate load. */
    movabs $0x000FFFFFFFFFF000, %rax
    /* AT&T syntax: 'andq %rax, %rdi' means %rdi = %rdi & %rax. */
    andq %rax, %rdi
    movq %rdi, %cr3
    ret
