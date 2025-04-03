#ifndef KERNEL_CPU_H
#define KERNEL_CPU_H

#include <stdint.h>
#include <stddef.h>

/* Maximum number of CPUs supported */
#define MAX_CPUS 64

/* Macros to save and restore interrupts */
#define temp_irq_save(flags) ({ \
    ASSERT_IS_TYPE(uint64_t, flags); \
    (flags) = cpu_save_clear_interrupts(); \
    (flags); \
})
#define temp_irq_restore(flags) ({ \
    ASSERT_IS_TYPE(uint64_t, flags); \
    cpu_restore_interrupts(flags); \
})

/* Invalidate a single page in the TLB */
static inline void cpu_invlpg(uintptr_t addr) {
    __asm volatile("invlpg (%0)" :: "r" (addr) : "memory");
}

/* Function prototypes for the CPU routines implemented in assembly */
void cpu_disable_interrupts(void);
void cpu_enable_interrupts(void);
uint64_t cpu_save_clear_interrupts(void);
void cpu_restore_interrupts(uint64_t flags);

uint64_t cpu_read_stack_pointer(void);
void cpu_write_stack_pointer(uint64_t sp);

uint64_t cpu_read_msr(uint32_t msr_index);
void cpu_write_msr(uint32_t msr_index, uint64_t value);

uint64_t cpu_read_tsc(void);

uint64_t cpu_read_fsbase(void);
void cpu_write_fsbase(uint64_t value);
uint64_t cpu_read_gsbase(void);
void cpu_write_gsbase(uint64_t value);
uint64_t cpu_read_kernel_gsbase(void);
void cpu_write_kernel_gsbase(uint64_t value);

void cpu_load_gdt(void *gdt_ptr);
void cpu_load_idt(void *idt_ptr);
void cpu_load_tr(uint16_t selector);

void cpu_set_cs(uint16_t cs);
void cpu_set_ds(uint16_t ds);

uint64_t __read_cr0(void);
void __write_cr0(uint64_t value);
uint64_t __read_cr2(void);
uint64_t __read_cr3(void);
void __write_cr3(uint64_t value);
uint64_t __read_cr4(void);
void __write_cr4(uint64_t value);

uint64_t __xgetbv(uint32_t index);
void __xsetbv(uint32_t index, uint64_t value);

void __fxsave(void *region);
void __fxrstor(void *region);

void cpu_flush_tlb(void);

/* Syscall interface – here declared with a variadic prototype for flexibility */
long syscall(long code, ...);

uint32_t get_cpu_id(void);

#endif /* KERNEL_CPU_H */