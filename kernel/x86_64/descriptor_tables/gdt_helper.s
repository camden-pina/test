/* Use AT&T syntax by default in GAS. */
.text

/* Declare external symbol gdtr from our C code. */
.extern gdtr

.globl gdt_flush
gdt_flush:
    /* Load the new GDT using RIP-relative addressing. */
    lgdt gdtr(%rip)
    
    /* Load the TSS.
       The selector 0x40 corresponds to GDT entry 8 (8*8=0x40). */
    movw $0x40, %ax
    ltr %ax

    /* Prepare for a far jump:
       - Set new SS (offset 0x10 in the GDT) and RSP.
       - Push RFLAGS, new CS (offset 0x08 in the GDT), and the new RIP.
       - iretq will pop CS:RIP, transferring control. */
    movq %rsp, %rax
    pushq $0x10          /* New SS (kernel data descriptor at GDT offset 0x10) */
    pushq %rax           /* New RSP */
    pushfq               /* Push RFLAGS */
    pushq $0x08          /* New CS (kernel code descriptor at GDT offset 0x08) */
    
    leaq 1f(%rip), %rax
    pushq %rax
    
    /* iretq pops CS:RIP, effectively jumping to label 1. */
    iretq

1:
    ret
