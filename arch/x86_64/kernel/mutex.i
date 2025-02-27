# 0 "mutex.c"
# 1 "/home/camdenpina/Music/ModernOS/arch/x86_64/kernel//"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "mutex.c"
# 1 "./include/mutex.h" 1



# 1 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stdint.h" 1 3 4
# 11 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stdint.h" 3 4
# 1 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stdint-gcc.h" 1 3 4
# 34 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stdint-gcc.h" 3 4

# 34 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stdint-gcc.h" 3 4
typedef signed char int8_t;


typedef short int int16_t;


typedef int int32_t;


typedef long int int64_t;


typedef unsigned char uint8_t;


typedef short unsigned int uint16_t;


typedef unsigned int uint32_t;


typedef long unsigned int uint64_t;




typedef signed char int_least8_t;
typedef short int int_least16_t;
typedef int int_least32_t;
typedef long int int_least64_t;
typedef unsigned char uint_least8_t;
typedef short unsigned int uint_least16_t;
typedef unsigned int uint_least32_t;
typedef long unsigned int uint_least64_t;



typedef int int_fast8_t;
typedef int int_fast16_t;
typedef int int_fast32_t;
typedef long int int_fast64_t;
typedef unsigned int uint_fast8_t;
typedef unsigned int uint_fast16_t;
typedef unsigned int uint_fast32_t;
typedef long unsigned int uint_fast64_t;




typedef long int intptr_t;


typedef long unsigned int uintptr_t;




typedef long int intmax_t;
typedef long unsigned int uintmax_t;
# 12 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stdint.h" 2 3 4
# 5 "./include/mutex.h" 2
# 1 "./include/lock.h" 1






# 6 "./include/lock.h"
struct thread;
struct mtx;

struct lock_claim_list;
# 18 "./include/lock.h"
struct lock_object {
    const char *name;
    uint32_t flags;
    uint32_t data;
};
# 67 "./include/lock.h"
struct lock_claim_list *lock_claim_list_alloc();
void lock_claim_list_free(struct lock_claim_list **listp);
void lock_claim_list_add(struct lock_claim_list *list, struct lock_object *lock, uintptr_t how, const char *file, int line);
void lock_claim_list_remove(struct lock_claim_list *list, struct lock_object *lock);

struct spin_delay {
    uint32_t delay_count;
    uint32_t max_waits;


    uint32_t waits;
};
# 91 "./include/lock.h"
int spin_delay_wait(struct spin_delay *delay);
# 6 "./include/mutex.h" 2

typedef struct mtx {
    struct lock_object lo;
    volatile uintptr_t mtx_lock;
} mtx_t;
# 25 "./include/mutex.h"
void _mtx_init(mtx_t *mtx, uint32_t opts, const char *name);
# 2 "mutex.c" 2
# 1 "./include/atomic.h" 1
# 3 "mutex.c" 2
# 1 "./include/kernel.h" 1



# 1 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 1 3 4
# 145 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 3 4

# 145 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 3 4
typedef long int ptrdiff_t;
# 214 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 3 4
typedef long unsigned int size_t;
# 329 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 3 4
typedef int wchar_t;
# 425 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 3 4
typedef struct {
  long long __max_align_ll __attribute__((__aligned__(__alignof__(long long))));
  long double __max_align_ld __attribute__((__aligned__(__alignof__(long double))));
# 436 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 3 4
} max_align_t;
# 450 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 3 4
  typedef __typeof__(nullptr) nullptr_t;
# 5 "./include/kernel.h" 2




# 8 "./include/kernel.h"
typedef struct _register_t
{
    unsigned long long r15, r14, r13, r12, r11, r10, r9, r8;
    unsigned long long rdi, rsi, rbp, rbx, rdx, rcx, rax;
    unsigned long long interrupt, errorcode;
    unsigned long long rip, cs, rflags, user_rsp, ss;
} register_t;


typedef struct register16 {
    unsigned short di;
    unsigned short si;
    unsigned short bp;
    unsigned short sp;
    unsigned short bx;
    unsigned short dx;
    unsigned short cx;
    unsigned short ax;

    unsigned short ds;
    unsigned short es;
    unsigned short fs;
    unsigned short gs;
    unsigned short ss;
    unsigned short eflags;
}register16_t;
# 63 "./include/kernel.h"
typedef struct {
    unsigned char Magic[2];
    unsigned char Mode;
    unsigned char CharacterSize;
} psf1_header;

typedef struct {
    psf1_header* header;
    void* GlyphBuffer;
} psf1_font;

typedef struct {
    void* base_addr;
    size_t buffer_sz;
    unsigned int px_width;
    unsigned int px_height;
    unsigned int bpp;
    unsigned int pps;
} framebuffer;

typedef struct flexboot_header_t
{

    void* memoryMap;
    unsigned long long mapSize;
    unsigned long long mapKey;
    unsigned long long descriptorSize;
    unsigned long long descriptorVersion;


    framebuffer* fb;

    psf1_font* font;


    unsigned long long* rsdp_addr;


    unsigned long long kernel_offset;
} flexboot_header_t;

typedef void (*KernelMainFunc)(
    flexboot_header_t* header
);

void kern_main(flexboot_header_t* header);
# 4 "mutex.c" 2

# 1 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 1 3 4
# 6 "mutex.c" 2
# 1 "./include/proc.h" 1



typedef struct thread {
    uint32_t flags;
    mtx_t lock;

    uintptr_t kstack_base;
    size_t kstack_size;


    int cpu_id;
    volatile uint32_t flags2;
    uint16_t : 16;
    uint8_t pri_base;
    uint8_t piority;


    int lock_count;
    int spin_count;
    int crit_level;
} thread_t;

void critical_enter();
void critical_exit();
# 7 "mutex.c" 2
# 1 "./include/percpu.h" 1




# 1 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h" 1 3 4
# 6 "./include/percpu.h" 2
# 1 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stdbool.h" 1 3 4
# 7 "./include/percpu.h" 2


struct thread;
struct proc;

struct address_space;
struct lock_claim_list;

struct percpu {
  uint32_t id;
  int intr_level;
  uintptr_t self;
  struct address_space *space;
  struct thread *thread;
  struct proc *proc;


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
# 8 "mutex.c" 2
# 1 "./include/panic.h" 1
# 18 "./include/panic.h"
void panic_early_init();
_Noreturn void panic(const char *fmt, ...);
# 9 "mutex.c" 2
# 31 "mutex.c"
static inline void spinlock_enter() {
    if (__builtin_expect((((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 32 "mutex.c" 3 4
       __builtin_offsetof (
# 32 "mutex.c"
       struct percpu
# 32 "mutex.c" 3 4
       , 
# 32 "mutex.c"
       thread
# 32 "mutex.c" 3 4
       )
# 32 "mutex.c"
       )); __v; })) != 
# 32 "mutex.c" 3 4
       ((void *)0)
# 32 "mutex.c"
       ), 1)) {
        __atomic_fetch_add(&((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 33 "mutex.c" 3 4
       __builtin_offsetof (
# 33 "mutex.c"
       struct percpu
# 33 "mutex.c" 3 4
       , 
# 33 "mutex.c"
       thread
# 33 "mutex.c" 3 4
       )
# 33 "mutex.c"
       )); __v; }))->spin_count, 1, 5);
    }
    critical_enter();
}

static inline void spinlock_exit() {
    if (__builtin_expect((((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 39 "mutex.c" 3 4
       __builtin_offsetof (
# 39 "mutex.c"
       struct percpu
# 39 "mutex.c" 3 4
       , 
# 39 "mutex.c"
       thread
# 39 "mutex.c" 3 4
       )
# 39 "mutex.c"
       )); __v; })) != 
# 39 "mutex.c" 3 4
       ((void *)0)
# 39 "mutex.c"
       ), 1)) {
        if (!(((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 40 "mutex.c" 3 4
       __builtin_offsetof (
# 40 "mutex.c"
       struct percpu
# 40 "mutex.c" 3 4
       , 
# 40 "mutex.c"
       thread
# 40 "mutex.c" 3 4
       )
# 40 "mutex.c"
       )); __v; }))->spin_count > 0)) panic("assertion failed: " "spinlock_exit() with no spin locks held" ", file %s, line %d", "mutex.c", 40);;
        __atomic_fetch_sub(&((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 41 "mutex.c" 3 4
       __builtin_offsetof (
# 41 "mutex.c"
       struct percpu
# 41 "mutex.c" 3 4
       , 
# 41 "mutex.c"
       thread
# 41 "mutex.c" 3 4
       )
# 41 "mutex.c"
       )); __v; }))->spin_count, 1, 5);
    }
    critical_exit();
}

uint32_t _mtx_opts_to_lockobject_flags(uint32_t opts) {
    uint32_t flags = 0x1000;

    if (opts & 0x1) {
        flags |= (0x0001);
        opts &= ~0x4;
    } else {
        flags |= (0x0002);
    }

    flags |= opts & 0x2 ? 0x0100 : 0;
    flags |= opts & 0x8 ? 0x0200 : 0;
    flags |= opts & 0x4 ? 0x0400 : 0;

    return flags;
}

void _mtx_init(mtx_t *mtx, uint32_t opts, const char *name) {
    mtx->lo.name = name;
    mtx->lo.flags = _mtx_opts_to_lockobject_flags(opts);
    mtx->lo.data = 0;
    mtx->mtx_lock = 0x00;
}

thread_t *_mtx_owner(mtx_t *mtx) {
    uintptr_t mtx_lock = mtx->mtx_lock;
    if (!(mtx_lock != 0x02)) panic("assertion failed: " "_mtx_destroy() on locked mutex" ", file %s, line %d", "mutex.c", 72);;
    return ((thread_t *) ((mtx_lock) & ~0x07));
}

void _mtx_spin_lock(mtx_t *mtx, const char *file, int line) {
  thread_t *owner = ((thread_t *) (((mtx)->mtx_lock) & ~0x07));
  if (!(mtx->mtx_lock != 0x02)) panic("assertion failed: " "_mtx_spin_lock() on destroyed mutex, %s:%d" ", file %s, line %d", file, line , "mutex.c", 78);;
  if (!(((&(mtx)->lo)->flags & 0x00ff) == (0x0001))) panic("assertion failed: " "_mtx_spin_lock() on non-spin mutex, %s:%d" ", file %s, line %d", file, line , "mutex.c", 79);;

  spinlock_enter();
  if (__builtin_expect((((struct lock_claim_list *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 82 "mutex.c" 3 4
 __builtin_offsetof (
# 82 "mutex.c"
 struct percpu
# 82 "mutex.c" 3 4
 , 
# 82 "mutex.c"
 spin_claims
# 82 "mutex.c" 3 4
 )
# 82 "mutex.c"
 )); __v; })) != 
# 82 "mutex.c" 3 4
 ((void *)0)
# 82 "mutex.c"
 ), 1)) lock_claim_list_add(((struct lock_claim_list *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 82 "mutex.c" 3 4
 __builtin_offsetof (
# 82 "mutex.c"
 struct percpu
# 82 "mutex.c" 3 4
 , 
# 82 "mutex.c"
 spin_claims
# 82 "mutex.c" 3 4
 )
# 82 "mutex.c"
 )); __v; })), &mtx->lo, 0, file, line);

  if (((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 84 "mutex.c" 3 4
     __builtin_offsetof (
# 84 "mutex.c"
     struct percpu
# 84 "mutex.c" 3 4
     , 
# 84 "mutex.c"
     thread
# 84 "mutex.c" 3 4
     )
# 84 "mutex.c"
     )); __v; })) != 
# 84 "mutex.c" 3 4
                  ((void *)0) 
# 84 "mutex.c"
                       && ((thread_t *) (((mtx)->mtx_lock) & ~0x07)) == ((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 84 "mutex.c" 3 4
                                                __builtin_offsetof (
# 84 "mutex.c"
                                                struct percpu
# 84 "mutex.c" 3 4
                                                , 
# 84 "mutex.c"
                                                thread
# 84 "mutex.c" 3 4
                                                )
# 84 "mutex.c"
                                                )); __v; }))) {
    if (!(((&(mtx)->lo)->flags & 0xffff) & 0x0400)) panic("assertion failed: " "_mtx_wait_lock() on non-recursive mutex, %s:%d" ", file %s, line %d", file, line , "mutex.c", 85);;

    mtx->mtx_lock |= 0x04;
    mtx->lo.data++;
    ((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 89 "mutex.c" 3 4
   __builtin_offsetof (
# 89 "mutex.c"
   struct percpu
# 89 "mutex.c" 3 4
   , 
# 89 "mutex.c"
   thread
# 89 "mutex.c" 3 4
   )
# 89 "mutex.c"
   )); __v; }))->lock_count++;
    return;
  }

  struct spin_delay delay = ((struct spin_delay) { .delay_count = (100), .max_waits = (0xffffffffU
# 93 "mutex.c"
                           ), .waits = 0, });
  uintptr_t mtx_lock = ((uintptr_t)(((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 94 "mutex.c" 3 4
                      __builtin_offsetof (
# 94 "mutex.c"
                      struct percpu
# 94 "mutex.c" 3 4
                      , 
# 94 "mutex.c"
                      thread
# 94 "mutex.c" 3 4
                      )
# 94 "mutex.c"
                      )); __v; }))) | ((0x01) & 0x07));
  for (;;) {


    if (({ __typeof__(*(&mtx->mtx_lock)) __old = (0x00); __atomic_compare_exchange_n(&mtx->mtx_lock, &__old, mtx_lock, false, 2, 2); })) {
      break;
    }
    while (__atomic_load_n(&mtx->mtx_lock, 0) != 0x00) {
      if (!spin_delay_wait(&delay)) {

        panic("spin mutex deadlock, %s:%d", file, line);
      }
    }
  }
  mtx->lo.data = 1;
}

void _mtx_spin_unlock(mtx_t *mtx, const char *file, int line) {
  thread_t *owner = ((thread_t *) (((mtx)->mtx_lock) & ~0x07));
  if (!(mtx->mtx_lock != 0x02)) panic("assertion failed: " "_mtx_spin_unlock() on destroyed mutex" ", file %s, line %d", "mutex.c", 113);;
  if (!(((&(mtx)->lo)->flags & 0x00ff) == (0x0001))) panic("assertion failed: " "_mtx_spin_unlock() on non-spin mutex" ", file %s, line %d", "mutex.c", 114);;
  if (!(owner == ((struct thread *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 115 "mutex.c" 3 4
 __builtin_offsetof (
# 115 "mutex.c"
 struct percpu
# 115 "mutex.c" 3 4
 , 
# 115 "mutex.c"
 thread
# 115 "mutex.c" 3 4
 )
# 115 "mutex.c"
 )); __v; })))) panic("assertion failed: " "_mtx_spin_unlock() on unowned mutex" ", file %s, line %d", "mutex.c", 115);;

  mtx->lo.data--;
  if (mtx->mtx_lock & 0x04 && mtx->lo.data > 0) {
    if (!(((&(mtx)->lo)->flags & 0xffff) & 0x0400)) panic("assertion failed: " "_mtx_spin_unlock() on non-recursive mutex" ", file %s, line %d", "mutex.c", 119);;
    if (mtx->lo.data == 1) {
      mtx->mtx_lock &= ~0x04;
    }
    return;
  }

  if (!(mtx->lo.data == 0)) panic("assertion failed: " "_mtx_spin_unlock() expected 0 count, got %d" ", file %s, line %d", mtx->lo.data , "mutex.c", 126);;
  __atomic_store_n(&mtx->mtx_lock, 0x00, 3);

  if (__builtin_expect((((struct lock_claim_list *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 129 "mutex.c" 3 4
 __builtin_offsetof (
# 129 "mutex.c"
 struct percpu
# 129 "mutex.c" 3 4
 , 
# 129 "mutex.c"
 spin_claims
# 129 "mutex.c" 3 4
 )
# 129 "mutex.c"
 )); __v; })) != 
# 129 "mutex.c" 3 4
 ((void *)0)
# 129 "mutex.c"
 ), 1)) lock_claim_list_remove(((struct lock_claim_list *) ({ register uint64_t __v; __asm("mov %0, gs:%1" : "=r" (__v) : "i" (
# 129 "mutex.c" 3 4
 __builtin_offsetof (
# 129 "mutex.c"
 struct percpu
# 129 "mutex.c" 3 4
 , 
# 129 "mutex.c"
 spin_claims
# 129 "mutex.c" 3 4
 )
# 129 "mutex.c"
 )); __v; })), &mtx->lo);
  spinlock_exit();
}
