.extern IDTR

.globl idt_flush
idt_flush:
  lidt (IDTR)
  ret
