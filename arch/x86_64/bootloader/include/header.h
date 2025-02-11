#ifndef HEADER_H
#define HEADER_H

#include <efi.h>
#include <efilib.h>
#include "bootloader.h"

/* In this simple example the kernel image itself is assumed to be a UEFI application.
   Thus, we do not manually parse an ELF header. If needed, you could add routines here to
   parse a custom boot header. For now, we simply define a boot header structure. */
typedef struct boot_header {
    UINT32 magic;
    void* kernel_entry;
    /* Additional fields (e.g. memory map, framebuffer info) can be added here */
} boot_header_t;

#define BOOT_HEADER_MAGIC 0x1BADB002

/* For this UEFI loader, we simply use the kernel image loaded by UEFI LoadImage.
   If you wish to do custom header parsing, add functions here. */
boot_header_t* get_boot_header(void);
int validate_boot_header(boot_header_t* hdr);

#endif // HEADER_H

