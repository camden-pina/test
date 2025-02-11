#ifndef _KERNEL_H
#define _KERNEL_H 1

//#include "Sys32/system.h"
// Register structs for interrupt/exception
typedef struct _register_t
{
    unsigned long long r15, r14, r13, r12, r11, r10, r9, r8;
    unsigned long long rdi, rsi, rbp, rbx, rdx, rcx, rax;
    unsigned long long interrupt, errorcode;
    unsigned long long rip, cs, rflags, user_rsp, ss;
} register_t;

// Register structs for bios service
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

typedef struct flexboot_header_t
{
    // EfiMemoryMap
    void* memoryMap;
    unsigned long long mapSize;
    unsigned long long mapKey;
    unsigned long long descriptorSize;
    unsigned long long descriptorVersion;

    // GOP
    unsigned int* fb;
    unsigned long long fb_sz;
    unsigned long long fb_w;
    unsigned long long fb_h;
    unsigned long long fb_bpp;
    unsigned long long fb_pps;

    // ACPI
    unsigned long long* rsdp_addr;
} flexboot_header_t;

typedef void (*KernelMainFunc)(
    flexboot_header_t* header
);

void kern_main(flexboot_header_t* header);

#endif