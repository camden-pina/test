/*
#include <mm/vmalloc.h>

#define ASSERT(x) kassert(x)
#define DPRINTF(x, ...) kprintf(x, ##__VA_ARGS__)
#define EPRINTF(fmt, ...) kprintf("vmalloc: %s: " fmt, __func__, ##__VA_ARGS__)
#define DPANICF(x, ...) panic(x, ##__VA_ARGS__)
#define ALLOC_ERROR(msg, ...) panic(msg, ##__VA_ARGS__)

#define do_align(x, al) ((al) > 0 ? (align(x, al)) : (x))

// these are the default hints for different combinations of vm flags
// they are used as a starting point for the kernel when searching for
// a free region
#define HINT_USER_DEFAULT   0x0000000050000000ULL // for VM_USER
#define HINT_USER_MALLOC    0x0000050000000000ULL // for VM_USER|VM_MALLOC
#define HINT_USER_STACK     0x0000800000000000ULL // for VM_USER|VM_STACK
#define HINT_KERNEL_DEFAULT 0xFFFFC00000000000ULL // for no flags
#define HINT_KERNEL_MALLOC  0xFFFFC01000000000ULL // for VM_MALLOC
#define HINT_KERNEL_STACK   0xFFFFFF8040000000ULL // for VM_STACK

extern uintptr_t entry_initial_stack_top;
address_space_t *default_user_space;
address_space_t *kernel_space;

void init_address_space() {
      // the page tables are still pretty much the same as what the bootloader set up for us
  //
  //   0x0000000000000000 - +1Gi           | identity mapped
  //   +1GB - 0x00007FFFFFFFFFFF           | unmapped
  //       ...
  //   === kernel mappings ===
  //   0xFFFF800000000000 - +1Mi           | mapped 0-1Mi
  //   kernel_code_start - kernel_code_end | kernel code (rw)
  //   kernel_code_end - kernel_data_end   | kernel data (rw)
  //       ...
  //   0xFFFFFF8000400000 - +6Mi           | kernel heap (rw)
  //       ...
  //   0xFFFFFF8000C00000 - +rsvd size     | kernel reserved (--)
  //

  init_recursive_pgtable();
}
*/