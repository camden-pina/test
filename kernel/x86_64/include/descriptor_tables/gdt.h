#ifndef _GDT_H
#define _GDT_H 1

#include "../kernel.h"
#include <string.h>

// From:
// http://www.cocoawithlove.com/2008/04/using-pointers-to-recast-in-c-is-bad.html
#define UNION_CAST(x, destType) \
  (((union {                    \
     __typeof__(x) a;           \
     destType b;                \
   })x).b)

#define KERNEL_CS 0x08
#define GDT_KERNEL_DS 0x10
#define GDT_TSS 0x18

void gdt_init(void);

#endif
