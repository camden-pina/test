#ifndef KERNEL_REF_H
#define KERNEL_REF_H

// https://www.open-std.org/JTC1/SC22/WG21/docs/papers/2007/n2167.pdf

#define __move    // identifies a pointer as a moved reference (ownership transferred)
#define __ref     // marks a refcounted parameter as a normal pointer (ownership not transferred)

typedef volatile int refcount_t;

#define _refname refcount
#define _refcount refcount_t _refname

#endif