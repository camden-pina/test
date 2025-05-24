#ifndef _UNISTD_H
#define _UNISTD_H
#include <stddef.h>
#include <stdint.h>

/* syscall numbers – must match your kernel table */
#define SYS_exit   1
#define SYS_fork   2
#define SYS_execve 3
#define SYS_wait   4
#define SYS_write  5

long  write(int fd,const void*buf,size_t len);
void  _exit(int status) __attribute__((noreturn));
#endif

