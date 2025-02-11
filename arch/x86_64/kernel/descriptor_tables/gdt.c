#include <descriptor_tables/gdt.h>

#include <stdio.h>

typedef struct segment_descriptor_t
{
    unsigned short  limit_low;
    unsigned short  base_low;
    unsigned char   base_middle;
    unsigned char   type : 4;
    unsigned char   s : 1;       // descriptor type (0 = system, 1 = code/data)
    unsigned char   dpl : 2;     // descriptor priviledge level
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
} __attribute__((packed)) tss;

struct GDTR {
    unsigned short      limit;
    unsigned long long  base;
} __attribute__((packed)) GDTR;

static segment_descriptor_t GDT[10];

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

void gdt_init(void) {

    gdt_set_entry(0x00, 0x00, 0x00000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);         // 0x00 null descriptor
    gdt_set_entry(0x01, 0x00, 0xFFFFFFFF, 0x0B, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x08 code descriptor
    gdt_set_entry(0x02, 0x00, 0xFFFFFFFF, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x10 kernel data
    gdt_set_entry(0x03, 0x00, 0x00000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);    // 0x18 null (user)
    gdt_set_entry(0x04, 0x00, 0xFFFFFFFF, 0x02, 0x01, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x20 user data
    gdt_set_entry(0x05, 0x00, 0xFFFFFFFF, 0x0A, 0x01, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x28 user code
    gdt_set_entry(0x06, 0x00, 0xFFFFFFFF, 0x02, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x30 ovmf data
    gdt_set_entry(0x07, 0x00, 0xFFFFFFFF, 0x0A, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x38 ovmf code
    
    memset((void*)&tss, 0, sizeof(tss));
    unsigned long long tss_base = ((unsigned long long)&tss);
    
    memset(&GDT[8], 0, sizeof(segment_descriptor_t));
    GDT[8].base_low = tss_base & 0xffff;
    GDT[8].base_middle = (tss_base >> 16) & 0xff;
    GDT[8].base_high = (tss_base >> 24) & 0xff;
    GDT[8].limit_low = sizeof(tss);
    GDT[8].type = 0x09;
    GDT[8].p = 0x01;
    GDT[8].g = 0x01;

    memset(&GDT[9], 0, sizeof(segment_descriptor_t));
    GDT[9].limit_low = (tss_base >> 32) & 0xffff;
    GDT[9].base_low = (tss_base >> 48) & 0xffff;

    GDTR.limit = sizeof(GDT) - 1;
    GDTR.base = (unsigned long long)&GDT[0];

    gdt_flush();
}

/* ########## DEBUG ##########

void print_entry(void* entry)
{
    segment_descriptor_t* descriptor = (segment_descriptor_t*)entry;

    printf("\nLimit_Low: %x\n\r", descriptor->limit_low);
    printf("Base_Low: %x\n\r", descriptor->base_low);
    printf("Base_Middle: %x\n\r", descriptor->base_middle);
    printf("Type: %x\n\r", descriptor->type);
    printf("S: %x\n\r", descriptor->s);
    printf("DPL: %x\n\r", descriptor->dpl);
    printf("P: %x\n\r", descriptor->p);
    printf("Limit_high: %x\n\r", descriptor->limit_high);
    printf("AVL: %x\n\r", descriptor->avl);
    printf("L: %x\n\r", descriptor->l);
    printf("DB: %x\n\r", descriptor->db);
    printf("G: %x\n\r", descriptor->g);
    printf("Base_High: %x\n\r", descriptor->base_high);
}

*/








/*
#include <descriptor_tables/gdt.h>

#include <stdio.h>

typedef struct segment_descriptor_t
{
    unsigned short  limit_low;
    unsigned short  base_low;
    unsigned char   base_middle;
    unsigned char   type : 4;
    unsigned char   s : 1;       // descriptor type (0 = system, 1 = code/data)
    unsigned char   dpl : 2;     // descriptor priviledge level
    unsigned char   p : 1;       // segment present
    unsigned char   limit_high : 4;
    unsigned char   avl : 1;     // available for use by system software
    unsigned char   l : 1;       // 64-bit code segment (IA-32e mode only)
    unsigned char   db : 1;      // Default operation size (0 = 16, 1 = 32)
    unsigned char   g : 1;       // granularity
    unsigned char   base_high;
} __attribute__((packed)) segment_descriptor_t;

unsigned short  limit_low;      0
unsigned short  base_low;       0
unsigned char   base_middle;    0
unsigned char   type : 4;       1001
unsigned char   s : 1;          1
unsigned char   dpl : 2;        01
unsigned char   p : 1;          0
unsigned char   limit_high : 4; 1010
unsigned char   avl : 1;        0
unsigned char   l : 1;          0
unsigned char   db : 1;         0
unsigned char   g : 1;          0
unsigned char   base_high;      0

unsigned short  limit_low;      0
unsigned short  base_low;       0
unsigned char   base_middle;    0
unsigned char   type : 4;       0011
unsigned char   s : 1;          1
unsigned char   dpl : 2;        0
unsigned char   p : 1;          1
unsigned char   limit_high : 4; 0
unsigned char   avl : 1;        0
unsigned char   l : 1;          1
unsigned char   db : 1;         0
unsigned char   g : 1;          1
unsigned char   base_high;      0

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
} __attribute__((packed)) tss;

struct GDTR {
    unsigned short      limit;
    unsigned long long  base;
} __attribute__((packed)) GDTR;

static segment_descriptor_t GDT[10];

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
#pragma pack (1)
struct GDTEntry
{
    unsigned short limit0;
    unsigned short base0;
    unsigned char base1;
    unsigned char accessbyte;
    unsigned char limit1_flags;
    unsigned char base2;
};

__attribute__((aligned(4096)))
struct
{
    struct GDTEntry Null;
    struct GDTEntry KernelCode;
    struct GDTEntry KernelDate;
    struct GDTEntry UserNull;
    struct GDTEntry UserCode;
    struct GDTEntry UserData;
} DefaultGDT = {
    { 0, 0, 0, 0x00, 0x00, 0 },
    { 0, 0, 0, 0x9a, 0xa0, 0 },
    { 0, 0, 0, 0x92, 0xa0, 0 },
    { 0, 0, 0, 0x00, 0x00, 0 },
    { 0, 0, 0, 0x9a, 0xa0, 0 },
    { 0, 0, 0, 0x92, 0xa0, 0 },
};

#pragma pack ()

void gdt_init(void)
{

    gdt_set_entry(0x00, 0x00, 0x00000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);         // 0x00 null descriptor
    gdt_set_entry(0x01, 0x00, 0x00000000, 0x3E9, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x08 code descriptor
    gdt_set_entry(0x02, 0x00, 0x00000000, 0x3E9, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x10 kernel data
    gdt_set_entry(0x03, 0x00, 0x00000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);    // 0x18 null (user)
    gdt_set_entry(0x04, 0x00, 0x00000000, 0x02, 0x01, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x20 user data
    gdt_set_entry(0x05, 0x00, 0x00000000, 0x0A, 0x01, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x28 user code
    gdt_set_entry(0x06, 0x00, 0x00000000, 0x02, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x30 ovmf data
    gdt_set_entry(0x07, 0x00, 0x00000000, 0x0A, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01);    // 0x38 ovmf code
    
    memset((void*)&tss, 0, sizeof(tss));
    unsigned long long tss_base = ((unsigned long long)&tss);
    
    memset(&GDT[8], 0, sizeof(segment_descriptor_t));
    GDT[8].base_low = tss_base & 0xffff;
    GDT[8].base_middle = (tss_base >> 16) & 0xff;
    GDT[8].base_high = (tss_base >> 24) & 0xff;
    GDT[8].limit_low = sizeof(tss);
    GDT[8].type = 0x09;
    GDT[8].p = 0x01;
    GDT[8].g = 0x01;

    memset(&GDT[9], 0, sizeof(segment_descriptor_t));
    GDT[9].limit_low = (tss_base >> 32) & 0xffff;
    GDT[9].base_low = (tss_base >> 48) & 0xffff;

    GDTR.limit = sizeof(GDT) - 1;
    GDTR.base = (unsigned long long)&GDT;

    gdt_flush();
}


void print_entry(void* entry)
{
    segment_descriptor_t* descriptor = (segment_descriptor_t*)entry;

    printf("\nLimit_Low: %x\n\r", descriptor->limit_low);
    printf("Base_Low: %x\n\r", descriptor->base_low);
    printf("Base_Middle: %x\n\r", descriptor->base_middle);
    printf("Type: %x\n\r", descriptor->type);
    printf("S: %x\n\r", descriptor->s);
    printf("DPL: %x\n\r", descriptor->dpl);
    printf("P: %x\n\r", descriptor->p);
    printf("Limit_high: %x\n\r", descriptor->limit_high);
    printf("AVL: %x\n\r", descriptor->avl);
    printf("L: %x\n\r", descriptor->l);
    printf("DB: %x\n\r", descriptor->db);
    printf("G: %x\n\r", descriptor->g);
    printf("Base_High: %x\n\r", descriptor->base_high);
}

*/