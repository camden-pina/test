#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

/* Remove the __attribute__((sysv_abi)) because the default on x86-64 Linux is System V */
extern long syscall6(long number, long a1, long a2, long a3, long a4, long a5, long a6);

int main(void)
{
    long fd = 4;

    asm volatile ("mov $0xdeadbeef, %%rsi" ::: "rsi");

    syscall6(SYS_write, fd, 0, 3, 0, 0, 0);
    while (1) {
    }
    exit(0);
}
