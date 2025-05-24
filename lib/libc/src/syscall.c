#include "syscall.h"
#include <unistd.h>
#include <stddef.h>

long write(int fd, const void *buf, size_t len)
{
    return 1;
}

void _exit(int status)
{
    for (;;)
        __asm__("hlt");
}
