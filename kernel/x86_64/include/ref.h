#ifndef KERNEL_REF_H
#define KERNEL_REF_H

typedef volatile int refcount_t;

typedef int refcount;
#define _refname refcount
#define _refcount refcount_t _refname

#endif
