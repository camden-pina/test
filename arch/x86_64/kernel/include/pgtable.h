#ifndef KERNEL_PGTABLE_H
#define KERNEL_PGTABLE_H

#include <kernel.h>
#include <mm_types.h>

void *early_map_entries(uintptr_t vaddr, uintptr_t paddr, size_t count, uint32_t vm_flags);

#endif
