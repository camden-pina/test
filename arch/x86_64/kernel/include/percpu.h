#ifndef KERNEL_PERCPU_H
#define KERNEL_PERCPU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// struct cpu_info;
struct thread;
struct proc;
// struct sched;
struct address_space;
struct lock_claim_list;

struct percpu {
  uint32_t id;
  int intr_level;
  uintptr_t self;
  struct address_space *space;
  struct thread *thread;
  struct proc *proc;
  // struct sched *sched;
  // struct cpu_info *info;
  struct lock_claim_list *spin_claims;
  uintptr_t user_sp;
  uintptr_t kernel_sp;
  uint64_t *tss_rsp0_ptr;
  uintptr_t irq_stack_top;
  uint64_t scratch_rax;
  uint64_t rflags;
  void *gdt;
  void *tss;
} __attribute__((aligned(128)));

/* 
 * Corrected "percpu" inline-assembly macros for x86_64 (GCC/Clang, AT&T syntax).
 * We assume you are using the default AT&T syntax, which means:
 *   movl source, destination
 *   movq source, destination
 *   and a segment override is typically `%%gs:`.
 */

 #define __percpu_get_u32(member)                                  \
 ({                                                                \
     register uint32_t __v;                                        \
     __asm__ __volatile__ (                                        \
         "movl %%gs:%c1, %0"                                       \
         : "=r" (__v)                                              \
         : "i" (offsetof(struct percpu, member))                   \
     );                                                            \
     __v;                                                          \
 })
 
 #define __percpu_get_u64(member)                                  \
 ({                                                                \
     register uint64_t __v;                                        \
     __asm__ __volatile__ (                                        \
         "movq %%gs:%c1, %0"                                       \
         : "=r" (__v)                                              \
         : "i" (offsetof(struct percpu, member))                   \
     );                                                            \
     __v;                                                          \
 })
 
 #define __percpu_set_u32(member, val)                             \
 ({                                                                \
     register uint32_t __tmp = (uint32_t)(val);                    \
     __asm__ __volatile__ (                                        \
         "movl %1, %%gs:%c0"                                       \
         : /* no outputs */                                        \
         : "i" (offsetof(struct percpu, member)), "r" (__tmp)      \
     );                                                            \
 })
 
 #define __percpu_set_u64(member, val)                             \
 ({                                                                \
     register uint64_t __tmp = (uint64_t)(val);                    \
     __asm__ __volatile__ (                                        \
         "movq %1, %%gs:%c0"                                       \
         : /* no outputs */                                        \
         : "i" (offsetof(struct percpu, member)), "r" (__tmp)      \
     );                                                            \
 })
 
 /* 
  * Now the original macros become:
  */
 
 #define PERCPU_ID          ((uint8_t)__percpu_get_u32(id))
 #define PERCPU_AREA        ((struct percpu *)__percpu_get_u64(self))
 #define PERCPU_RFLAGS      __percpu_get_u64(rflags)
 #define PERCPU_SET_RFLAGS(val)  __percpu_set_u64(rflags, val)
 
 #define curcpu_id ((uint8_t) __percpu_get_u32(id))

 #define curthread           ((struct thread *)__percpu_get_u64(thread))
 #define curcpu_spin_claims  ((struct lock_claim_list *)__percpu_get_u64(spin_claims))
 
#endif