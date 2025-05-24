#include "file/elf_loader.h"
#include "storage/fs/vfs.h"       // VFS functions: vfs_open, vfs_read, vfs_file_size, vfs_close
#include "mm/pmm.h"               // kmalloc, kfree
#include "printf.h"               // kprintf for logging
#include "panic.h"                // panic (if needed)
#include <string.h>               // memcpy, memset
#include "mm/vmem.h"              // Using the VMEM API functions: vmap_pages, etc.
#include "mm/pgtable.h"           // For page table definitions (if needed)
#include <task.h>
#include <storage/partition.h>
#include <mm_types.h>

#define EI_NIDENT 16
#define ELF_MAGIC 0x464C457FU  // 0x7F 'E' 'L' 'F'

// Standard ELF typedefs for 64-bit
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

/* ELF header structure */
typedef struct {
    unsigned char e_ident[EI_NIDENT]; /* ELF identification */
    Elf64_Half    e_type;             /* Object file type */
    Elf64_Half    e_machine;          /* Machine type */
    Elf64_Word    e_version;          /* Object file version */
    Elf64_Addr    e_entry;            /* Entry point address */
    Elf64_Off     e_phoff;            /* Program header offset */
    Elf64_Off     e_shoff;            /* Section header offset */
    Elf64_Word    e_flags;            /* Processor-specific flags */
    Elf64_Half    e_ehsize;           /* ELF header size */
    Elf64_Half    e_phentsize;        /* Size of program header entry */
    Elf64_Half    e_phnum;            /* Number of program header entries */
    Elf64_Half    e_shentsize;        /* Size of section header entry */
    Elf64_Half    e_shnum;            /* Number of section header entries */
    Elf64_Half    e_shstrndx;         /* Section name string table index */
} Elf64_Ehdr;

/* Program header constants */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7

/* Program header structure */
typedef struct {
    Elf64_Word  p_type;   /* Type of segment */
    Elf64_Word  p_flags;  /* Segment attributes */
    Elf64_Off   p_offset; /* Offset in file */
    Elf64_Addr  p_vaddr;  /* Virtual address in memory */
    Elf64_Addr  p_paddr;  /* Reserved */
    Elf64_Xword p_filesz; /* Size of segment in file */
    Elf64_Xword p_memsz;  /* Size of segment in memory */
    Elf64_Xword p_align;  /* Alignment of segment */
} Elf64_Phdr;

/* Helper to align values upward */
static inline uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/* ELF segment flag definitions (as per ELF standard) */
#define PF_X 1
#define PF_W 2
#define PF_R 4

/* User stack configuration */
#define USER_STACK_SIZE (8 * 1024 * 1024)          // 8 MB stack
#define USER_STACK_TOP  0x00007ffffffff000ULL       // Arbitrarily chosen top-of-stack

/**
 * @brief Helper function that allocates pages and maps them in the process's address space.
 *
 * This function switches to the process's address space (using the provided address_space_t pointer),
 * allocates the required number of physical pages, and then maps them using vmap_pages().
 * It returns 0 on success and -1 on failure.
 *
 * @param as    Pointer to the process's address space.
 * @param hint  Desired starting virtual address.
 * @param size  Size of the memory region.
 * @param flags VMEM flags (e.g., VM_USER, VM_READ, VM_WRITE, VM_EXEC).
 * @param name  A string used for debugging purposes.
 * @return 0 on success, -1 on failure.
 */
static int vmem_alloc_range(address_space_t *as, uint64_t hint, uint64_t size, uint32_t flags, const char *name) {
    vm_set_current_space(as);
    size_t npages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    page_t *pages = alloc_pages(npages);
    if (!pages) {
        kprintf("vmem_alloc_range: failed to allocate %zu pages for mapping at 0x%lx\n", npages, hint);
        return -1;
    }
    uintptr_t mapped = vmap_pages(pages, hint, size, flags | VM_FIXED | VM_USER, name);
    if (mapped != hint) {
        kprintf("vmem_alloc_range: mapping returned 0x%lx, expected 0x%lx\n", mapped, hint);
        return -1;
    }
    return 0;
}

/**
 * @brief Loads an ELF binary from the given file path into the process's address space.
 *
 * This function opens the ELF file via VFS, reads its contents into a temporary buffer,
 * validates the ELF header, and then iterates through the program headers. For each
 * PT_LOAD segment, it calculates the page-aligned region, allocates physical pages and maps
 * them in the process's address space using vmap_pages, copies the segment data,
 * and zeros any extra space as needed.
 *
 * Finally, it sets the entry point and reserves a user stack in the process's address space.
 *
 * @param path           The path to the ELF binary.
 * @param as             Pointer to the process's address space.
 * @param entry          Pointer to store the ELF entry point address.
 * @param user_stack_top Pointer to store the top address of the allocated user stack.
 * @return 0 on success, -1 on failure.
 */
int load_elf_binary(const char *path, address_space_t *as, uint64_t *entry, uint64_t *user_stack_top) {
    kprintf("Loading ELF binary: %s\n", path);

    // For this example, we'll assume device #0 is the raw disk.
    block_device_t *raw_disk = get_block_device(0);
    if (!raw_disk) {
        panic("Error: raw disk not found");
    }
    kprintf("Raw disk '%s' with %llu sectors.\n",
            raw_disk->name, (unsigned long long)raw_disk->sector_count);

    // Perform partition scanning on the raw disk.
    block_list();
    int pcount = partition_scan(raw_disk);
    if (pcount < 0) {
        panic("No valid partition table or parse error.");
    }
    if (pcount == 0) {
        kprintf("No partitions found on the raw disk.\n");
        return -1;
    }
    kprintf("partition_scan found %d partition(s).\n", pcount);

    // After scanning, determine the new device count.
    int after_count = get_block_device_count();
    // Assume the last device registered is the one we want.
    int partition_index = after_count - 1;
    block_device_t *part_dev = get_block_device(partition_index);
    if (!part_dev) {
        panic("Couldn't get the newly registered partition device!");
    }
    kprintf("Attempting to mount FAT32 on device #%d ('%s').\n",
            partition_index, part_dev->name);

    // Mount the partition as the root filesystem under a mount point.
    if (vfs_mount("/drive0", part_dev, "fat32") != 0) {
        panic("Failed to mount the partition device as FAT32.");
    }

    path = strcat("/drive0", path);


    /* Open the ELF file using VFS */
    vfs_file_t *file = vfs_open(path, "r");
    if (!file) {
        kprintf("Failed to open ELF file: %s\n", path);
        return -1;
    }

    /* Determine file size and allocate a temporary buffer */
    uint32_t file_size = vfs_file_size(file);
    kprintf("ELF file size: %u bytes\n", file_size);
    uint8_t *file_buffer = (uint8_t*)kmalloc(file_size);
    if (!file_buffer) {
        kprintf("Memory allocation failed for ELF file buffer.\n");
        vfs_close(file);
        return -1;
    }

    /* Read the entire file in 4K chunks */
    uint32_t total_read = 0;
    const int chunk_size = 4096;
    int bytes_read = 0;
    while (total_read < file_size) {
        int to_read = (file_size - total_read < chunk_size) ? file_size - total_read : chunk_size;
        bytes_read = vfs_read(file, file_buffer + total_read, to_read);
        if (bytes_read < 0) {
            kprintf("Error reading ELF file: %s\n", path);
            kfree(file_buffer);
            vfs_close(file);
            return -1;
        }
        if (bytes_read == 0) {  // premature EOF
            break;
        }
        total_read += bytes_read;
    }
    vfs_close(file);
    if (total_read < file_size) {
        kprintf("Warning: Only read %u bytes out of %u from ELF file: %s\n",
                total_read, file_size, path);
    }

    /* Validate the ELF header */
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)file_buffer;
    uint32_t magic = *(uint32_t*)ehdr->e_ident;
    if (magic != ELF_MAGIC) {
        kprintf("Invalid ELF magic in file: %s\n", path);
        kfree(file_buffer);
        return -1;
    }
    if (ehdr->e_ident[4] != 2) {  // ELFCLASS64
        kprintf("ELF file is not 64-bit: %s\n", path);
        kfree(file_buffer);
        return -1;
    }
    kprintf("ELF header validated. Entry point: 0x%lx\n", ehdr->e_entry);

    /* Process each program header */
    Elf64_Phdr *phdr = (Elf64_Phdr*)(file_buffer + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;  // Skip non-loadable segments

        kprintf("Loading segment %d: offset=0x%lx, vaddr=0x%lx, filesz=0x%lx, memsz=0x%lx, align=0x%lx, flags=0x%x\n",
                i, phdr[i].p_offset, phdr[i].p_vaddr,
                phdr[i].p_filesz, phdr[i].p_memsz, phdr[i].p_align, phdr[i].p_flags);

        /* Calculate the aligned virtual region for the segment */
        uint64_t seg_start = phdr[i].p_vaddr;
        uint64_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
        uint64_t seg_page_start = seg_start & ~(PAGE_SIZE - 1);
        uint64_t seg_page_end = align_up(seg_end, PAGE_SIZE);
        uint64_t seg_size = seg_page_end - seg_page_start;

        /* Map ELF segment flags to VMEM flags */
        uint32_t mem_flags = VM_USER;
        if (phdr[i].p_flags & PF_R) mem_flags |= VM_READ;
        if (phdr[i].p_flags & PF_W) mem_flags |= VM_WRITE;
        if (phdr[i].p_flags & PF_X) mem_flags |= VM_EXEC;

        /* Allocate and map the region in the process's address space */
        if (vmem_alloc_range(as, seg_page_start, seg_size * 2, mem_flags, "ELF segment") < 0) {
            kprintf("Failed to allocate memory for segment %d at 0x%lx\n", i, seg_page_start);
            kfree(file_buffer);
            return -1;
        }

        vm_print_address_space();

        /* Copy the segment's file contents into the allocated memory.
         * Account for the offset of the segment within the first page.
         */
        size_t offset_in_page = phdr[i].p_vaddr - seg_page_start;
        uint8_t *dest = (uint8_t*)seg_page_start + offset_in_page;
        memcpy(dest, file_buffer + phdr[i].p_offset, phdr[i].p_filesz);

        /* Zero any extra space if p_memsz > p_filesz */
        if (phdr[i].p_memsz > phdr[i].p_filesz) {
            memset(dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
        }
    }

    /* Set the entry point for the process */
    *entry = ehdr->e_entry;
    kprintf("ELF binary loaded. Entry point: 0x%lx\n", *entry);

    /* Allocate a user stack.
     * Here we define a fixed region: a USER_STACK_SIZE block ending at USER_STACK_TOP.
     */
    uint64_t stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
    if (vmem_alloc_range(as, stack_bottom, USER_STACK_SIZE, VM_FIXED | VM_USER | VM_READ | VM_WRITE, "User stack") < 0) {
        kprintf("Failed to allocate user stack at 0x%lx\n", stack_bottom);
        kfree(file_buffer);
        return -1;
    }
    vm_print_address_space();
    *user_stack_top = USER_STACK_TOP;
    kprintf("User stack allocated from 0x%lx to 0x%lx\n", stack_bottom, USER_STACK_TOP);

    /* Clean up the temporary file buffer */
    kfree(file_buffer);
    return 0;
}
