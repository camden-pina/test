#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>
#include <stddef.h>

#include <mm_types.h>

/* 
 * Loads an ELF binary from the specified path into the given mm_struct.
 * On success, it sets *entry to the ELF entry point and *user_stack_top to the top
 * of the allocated user stack, returning 0. On failure, it returns -1.
 */
int load_elf_binary(const char *path, address_space_t *as, uint64_t *entry, uint64_t *user_stack_top);

#endif /* ELF_LOADER_H */
