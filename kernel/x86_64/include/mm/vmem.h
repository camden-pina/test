#ifndef _VMEM_H
#define _VMEM_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm_types.h>   // This brings in definitions for address_space_t, vm_mapping_t, page_t, etc.

/* Define off_t if not already defined */
#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef long off_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Public API for virtual memory mappings */

// Create various types of mappings in the current address space:
uintptr_t vmap_rsvd(uintptr_t hint, size_t size, uint32_t vm_flags, const char *name);
uintptr_t vmap_phys(uintptr_t phys_addr, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name);
uintptr_t vmap_pages(page_t *pages, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name);
uintptr_t vmap_anon(size_t vm_size, uintptr_t hint, size_t size, uint32_t vm_flags, const char *name);

// A simplified mmap interface (currently supports anonymous mapping only)
void *vm_mmap(uintptr_t addr, size_t len, int prot, int flags, int fd, off_t off);

// Change protection on an existing mapping.
int vmap_protect(uintptr_t vaddr, size_t len, int prot);

// Free a mapping in the current address space.
int vmap_free(uintptr_t vaddr, size_t len);

/* Kernel dynamic allocation via virtual mappings. The vmalloc interface
   creates a mapping in the kernel address space that you later free with vfree. */
void *vmalloc(size_t size, uint32_t vm_flags);
void vfree(void *ptr);

/* Address space management */
void init_default_mappings(void);
void vmem_init(void);
void init_ap_address_space();

address_space_t *vm_new_space(uintptr_t min_addr, uintptr_t max_addr, uintptr_t page_table);
address_space_t *vm_current_space(void);
void vm_set_current_space(address_space_t *space);

/* Debugging */
void vm_print_address_space(void);

uintptr_t get_default_ap_pml4();

void *ioremap(uintptr_t phys_addr, size_t size, char *name);
void iounmap(void *addr, size_t size);

/*
 * clone_kernel_space: Clone the kernel space mappings into a new page table.
 *
 * This function assumes that kernel mappings are located in the upper half of the
 * PML4 (i.e. indices 256 to 511). It copies those entries from the current page table
 * (which contains the shared kernel mappings) into the new page table provided.
 *
 * This is crucial so that new user processes can correctly reference kernel routines
 * and data without needing separate copies of the kernel mappings.
 */
uintptr_t clone_kernel_space(uint64_t *new_pml4);

#ifdef __cplusplus
}
#endif

#endif // _VMEM_H
