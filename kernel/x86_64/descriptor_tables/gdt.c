#include <descriptor_tables/gdt.h>
#include <printf.h>
#include <panic.h>
#include <string.h>  // for memset

/* Define the offset from entry_initial_stack_top to obtain the initial kernel stack pointer.
   This is equivalent to (TCB_SIZE + TRAPFRAME_SIZE) defined in your entry.s. */
#define STACK_TOP_OFF (0xA0 + 0xC0)  // 0x160

typedef struct segment_descriptor_t
{
    unsigned short  limit_low;
    unsigned short  base_low;
    unsigned char   base_middle;
    unsigned char   type : 4;
    unsigned char   s : 1;       // descriptor type (0 = system, 1 = code/data)
    unsigned char   dpl : 2;     // descriptor privilege level
    unsigned char   p : 1;       // segment present
    unsigned char   limit_high : 4;
    unsigned char   avl : 1;     // available for use by system software
    unsigned char   l : 1;       // 64-bit code segment (IA-32e mode only)
    unsigned char   db : 1;      // Default operation size (0 = 16, 1 = 32)
    unsigned char   g : 1;       // granularity
    unsigned char   base_high;
} __attribute__((packed)) segment_descriptor_t;

struct tss {
    unsigned int reserved0;
    unsigned int rsp0_low;
    unsigned int rsp0_high;
    unsigned int rsp1_low;
    unsigned int rsp1_high;
    unsigned int rsp2_low;
    unsigned int rsp2_high;
    unsigned int reserved1;
    unsigned int reserved2;
    unsigned int ist1_low;
    unsigned int ist1_high;
    unsigned int ist2_low;
    unsigned int ist2_high;
    unsigned int ist3_low;
    unsigned int ist3_high;
    unsigned int ist4_low;
    unsigned int ist4_high;
    unsigned int ist5_low;
    unsigned int ist5_high;
    unsigned int ist6_low;
    unsigned int ist6_high;
    unsigned int ist7_low;
    unsigned int ist7_high;
    unsigned int reserved3;
    unsigned int reserved4;
    unsigned short reserved5;
    unsigned short io_map_base_address;
} __attribute__((packed));

struct GDTR {
    unsigned short      limit;
    unsigned long long  base;
} __attribute__((packed));

static segment_descriptor_t GDT[10];
static struct tss tss_instance;

/* ------------------------------------------------------------------ */
/*  NEW: make the current RSP0 visible to low‑level stubs             */
/* ------------------------------------------------------------------ */
uint64_t kernel_rsp0 = 0;          /* exported symbol                 */

void set_kernel_rsp0(uint64_t rsp)
{
    kernel_rsp0      = rsp;        /* <<< keep a 64‑bit copy          */
    tss_instance.rsp0_low  = (uint32_t)(rsp & 0xFFFFFFFF);
    tss_instance.rsp0_high = (uint32_t)(rsp >> 32);
}


/* Global GDTR for use in the assembly routine (gdt_flush.s). */
struct GDTR gdtr;

static void gdt_set_entry(unsigned long long idx, unsigned long long base,
    unsigned long long limit, unsigned int type, unsigned char s,
    unsigned char dpl, unsigned char p, unsigned char avl, unsigned char l,
    unsigned char db, unsigned char g)
{
    memset(&GDT[idx], 0, sizeof(segment_descriptor_t));
    GDT[idx].limit_low      = (limit & 0xFFFF);
    GDT[idx].base_low       = (base & 0xFFFF);
    GDT[idx].base_middle    = ((base >> 16) & 0xFF);
    GDT[idx].type           = type;
    GDT[idx].s              = s;
    GDT[idx].dpl            = dpl;
    GDT[idx].p              = p;
    GDT[idx].limit_high     = ((limit >> 16) & 0xF);
    GDT[idx].avl            = avl;
    GDT[idx].l              = l;
    GDT[idx].db             = db;
    GDT[idx].g              = g;
    GDT[idx].base_high      = ((base >> 24) & 0xFF);
}

extern void gdt_flush(void);

/* Declare the external symbol for the top of the initial stack defined in entry.s */
extern unsigned char entry_initial_stack_top[];

void gdt_init(void) {

    /* Set up kernel GDT entries. */
    gdt_set_entry(0x00, 0x00, 0x00000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);         // 0x00 null descriptor
    gdt_set_entry(0x01, 0x00, 0xFFFFFFFF, 0x0B, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x08 kernel code descriptor
    gdt_set_entry(0x02, 0x00, 0xFFFFFFFF, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x10 kernel data descriptor
    gdt_set_entry(0x03, 0x00, 0x00000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);    // 0x18 null (user)
    gdt_set_entry(0x04, 0x00, 0xFFFFFFFF, 0x02, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01);

    // gdt_set_entry(0x04, 0x00, 0xFFFFFFFF, 0x02, 0x01, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x20 user data descriptor
    gdt_set_entry(0x05, 0x00, 0xFFFFFFFF, 0x0A, 0x01, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x28 user code descriptor
    // gdt_set_entry(0x06, 0x00, 0xFFFFFFFF, 0x02, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x30 ovmf data descriptor
    // gdt_set_entry(0x07, 0x00, 0xFFFFFFFF, 0x0A, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x38 ovmf code descriptor

    /* Initialize TSS to zero. */
    memset((void*)&tss_instance, 0, sizeof(tss_instance));
    
    /* Compute the initial kernel stack pointer:
       entry_initial_stack_top is defined in assembly; subtract STACK_TOP_OFF to get the stack pointer value. */
    unsigned long long kernel_stack = ((unsigned long long)entry_initial_stack_top) - STACK_TOP_OFF;
    tss_instance.rsp0_low  = (unsigned int)(kernel_stack & 0xFFFFFFFF);
    tss_instance.rsp0_high = (unsigned int)(kernel_stack >> 32);

    /* Create TSS descriptor spanning GDT entries 8 and 9. */
    unsigned long long tss_base = (unsigned long long)&tss_instance;
    
    /* First part of the TSS descriptor. */
    memset(&GDT[8], 0, sizeof(segment_descriptor_t));
    GDT[8].base_low    = tss_base & 0xFFFF;
    GDT[8].base_middle = (tss_base >> 16) & 0xFF;
    GDT[8].base_high   = (tss_base >> 24) & 0xFF;
    GDT[8].limit_low   = sizeof(tss_instance) & 0xFFFF;
    GDT[8].type        = 0x09;   // 64-bit available TSS
    GDT[8].p           = 0x01;
    GDT[8].g           = 0x00;   // TSS uses byte granularity

    /* Second part of the TSS descriptor. */
    memset(&GDT[9], 0, sizeof(segment_descriptor_t));
    GDT[9].limit_low   = (tss_base >> 32) & 0xFFFF;
    GDT[9].base_low    = (tss_base >> 48) & 0xFFFF;

    /* Set up the global GDTR. */
    gdtr.limit = sizeof(GDT) - 1;
    gdtr.base  = (unsigned long long)&GDT[0];

    /* Flush the GDT and load the new segments with a far jump using our assembly routine. */
    gdt_flush();
}
