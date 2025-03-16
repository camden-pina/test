#ifndef ATOMICS_H
#define ATOMICS_H

#include <stdbool.h>

/* 
 * Define memory order enum values.
 * In C11, these are provided by <stdatomic.h>.
 */
typedef enum {
    memory_order_relaxed = 0,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
} memory_order;

/* Define atomic_bool as a volatile int.
   We use int for simplicity (0 == false, 1 == true). */
typedef volatile int atomic_bool;

/* Macro to initialize an atomic variable.
   Example usage: atomic_bool flag = ATOMIC_VAR_INIT(false); */
#define ATOMIC_VAR_INIT(value) ((value) ? 1 : 0)

/* Initialize the atomic boolean */
static inline void atomic_init(atomic_bool *obj, bool value) {
    *obj = value ? 1 : 0;
}

/* Basic atomic load.
   The inline assembly forces an actual memory load (with a memory clobber). */
static inline bool atomic_load(const atomic_bool *obj) {
    int value;
    __asm__ __volatile__("movl %1, %0"
                         : "=r"(value)
                         : "m"(*obj)
                         : "memory");
    return value != 0;
}

/* Basic atomic store.
   This ensures that the store writes directly to memory. */
static inline void atomic_store(atomic_bool *obj, bool value) {
    int tmp = value ? 1 : 0;
    __asm__ __volatile__("movl %1, %0"
                         : "=m"(*obj)
                         : "r"(tmp)
                         : "memory");
}

/* Atomic exchange function using the lock-prefixed xchg instruction.
   This acts as a full barrier. */
static inline bool atomic_exchange(atomic_bool *obj, bool new_value) {
    int new_val = new_value ? 1 : 0;
    int old;
    __asm__ __volatile__("lock xchg %0, %1"
                         : "=r"(old), "+m"(*obj)
                         : "0"(new_val)
                         : "memory");
    return old != 0;
}

/* Atomic load with explicit memory ordering.
   For acquire or stronger ordering, we insert an mfence after the load. */
static inline bool atomic_load_explicit(const atomic_bool *obj, memory_order order) {
    bool value = atomic_load(obj);
    if (order == memory_order_acquire ||
        order == memory_order_seq_cst ||
        order == memory_order_acq_rel) {
        __asm__ __volatile__("mfence" ::: "memory");
    }
    return value;
}

/* Atomic store with explicit memory ordering.
   For release or stronger ordering, we insert an mfence before the store
   to ensure all previous writes complete before this store. */
static inline void atomic_store_explicit(atomic_bool *obj, bool value, memory_order order) {
    if (order == memory_order_release ||
        order == memory_order_seq_cst ||
        order == memory_order_acq_rel) {
        __asm__ __volatile__("mfence" ::: "memory");
    }
    atomic_store(obj, value);
}

#endif /* ATOMICS_H */
