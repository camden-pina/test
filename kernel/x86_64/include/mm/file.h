#ifndef KERNEL_MM_FILE_H
#define KERNEL_MM_FILE_H

#include <mm_types.h>

typedef struct vm_file {
    size_t size;             // size of the file
    size_t off;              // offset into the file
    size_t pg_size;          // size of each page

    struct vnode *vnode;     // backing vnode reg (null if anonymous)
    struct pgcache *pgcache; // the page cache (gloval)
} vm_file_t;

#endif
