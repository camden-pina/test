#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* System call numbers */
#define SYS_EXIT    1
#define SYS_FORK    2
#define SYS_EXECVE  3
#define SYS_WAIT    4
#define SYS_WRITE   5   /* for example, to write to console */

/* Number of implemented syscalls */
#define NR_SYSCALLS 6

/* Syscall handler function type */
typedef int64_t (*syscall_func_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* Syscall dispatch table (defined in syscall.c) */
extern syscall_func_t syscall_table[];

/* Syscall handler functions */
int64_t sys_exit(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_fork(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_execve(uint64_t pathname, uint64_t argv, uint64_t envp, uint64_t, uint64_t, uint64_t);
int64_t sys_wait(uint64_t pid, uint64_t status_ptr, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count, uint64_t, uint64_t, uint64_t);

/* Architecture-specific syscall initialization (to set up MSRs) */
void syscall_init(void);

#endif /* SYSCALL_H */
