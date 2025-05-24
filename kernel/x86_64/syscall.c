
#include "syscall.h"
#include "task.h"
#include "sched.h"
#include "file/elf_loader.h"
#include <stddef.h>
#include <string.h>
#include <context.h>
#include <mm/vmem.h>
#include <mm/pgtable.h>
#include <printf.h>
#include <mm/pmm.h>

/* System call dispatch table */
syscall_func_t syscall_table[NR_SYSCALLS] = {
    NULL,        /* 0 - unused */
    sys_exit,    /* 1 */
    sys_fork,    /* 2 */
    sys_execve,  /* 3 */
    sys_wait,    /* 4 */
    sys_write    /* 5 */
};

/*
 * syscall_dispatch:
 *
 * This function is called from the syscall_entry assembly stub.
 * It receives syscall parameters via registers, following the SysV ABI:
 *    - RDI: syscall number
 *    - RSI: argument 1
 *    - RDX: argument 2
 *    - RCX: argument 3
 *    - R8:  argument 4
 *    - R9:  argument 5
 *    - (and, if needed, a 7th parameter in the stack; here we assume 6 parameters)
 *
 * It then dispatches the call to the appropriate syscall handler from the syscall_table.
 *
 * Returns:
 *    The value returned by the syscall handler in RAX.
 */
#include <mm_types.h>
extern address_space_t *kernel_space;
#include <cpu/cpu.h>
#include <8250.h>


int64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2,
    uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    kprintf("sys num: %llu\n", num);

    if (num >= NR_SYSCALLS || !syscall_table[num]) {
        return -1;  // Invalid syscall number.
    }
    return syscall_table[num](arg1, arg2, arg3, arg4, arg5, arg6);
}

/* sys_exit: terminate current process */
int64_t sys_exit(uint64_t code, uint64_t arg1, uint64_t arg2,
                 uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    task_exit((int)code);
    return 0; // not reached
}

/* sys_fork: create a copy of the current process */
int64_t sys_fork(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    struct task_struct *parent = current_task;
    struct task_struct *child = alloc_task();
    if (!child) {
        return -1;  // ENOMEM
    }
    /* Set up child's links */
    child->parent = parent;
    child->mm = parent->mm;  // now an address_space_t*
    child->files = parent->files;
    // Add to parent's children list
    if (!parent->children.next) {
        parent->children.next = child;
        child->sibling.prev = (struct task_struct*)&parent->children;
    } else {
        struct task_struct *first_child = parent->children.next;
        child->sibling.next = first_child;
        first_child->sibling.prev = child;
        parent->children.next = child;
        child->sibling.prev = (struct task_struct*)&parent->children;
    }
    /* Duplicate context: use parent's user context to set child's */
    if (parent->context) {
        struct cpu_context *parent_frame = (struct cpu_context*)parent->context;
        init_task_context(child, (void*)parent_frame->rip, (void*)parent_frame->rsp, true);
        struct cpu_context *child_frame = (struct cpu_context*)child->context;
        *child_frame = *parent_frame;
        child_frame->rax = 0;  // child returns 0
    } else {
        void *rip = __builtin_return_address(0);
        void *rsp = (void*)0;
        init_task_context(child, rip, rsp, true);
        struct cpu_context *child_frame = (struct cpu_context*)child->context;
        child_frame->rax = 0;
    }
    /* Enqueue child on runqueue */
    child->state = TASK_READY;
    enqueue_task(child, get_cpu_id());
    return child->pid;
}

/* sys_execve: replace current process image with a new program */
int64_t sys_execve(uint64_t pathname, uint64_t argv, uint64_t envp,
                   uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    const char *path = (const char*)pathname;
    char *const *uargv = (char *const *)argv;
    char *const *uenvp = (char *const *)envp;
    struct task_struct *curr = current_task;
    
    /* Load new program into a *new* address space */
    address_space_t *old_as = (address_space_t *)curr->mm;
    address_space_t *new_as = (address_space_t*)kmalloc(sizeof(address_space_t));
    memset(new_as, 0, sizeof(*new_as));
    
    /* Allocate a new page table and initialize kernel space */
    new_as->page_table = (uintptr_t)allocate_table_page();
    new_as->page_table = clone_kernel_space((void*)new_as->page_table);
    
    uint64_t entry = 0, user_stack_top = 0;
    if (load_elf_binary(path, new_as, &entry, &user_stack_top) < 0) {
        kfree((void*)new_as->page_table);
        kfree(new_as);
        return -1;
    }
    
    /* Switch to new address space (CR3) */
    uintptr_t new_as_phys = virt_to_phys((void*)new_as->page_table);
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(new_as_phys) : "memory");
    
    /* Free old address space (note: this is a stub and does not free all resources) */
    kfree((void*)old_as->page_table);
    kfree(old_as);
    curr->mm = new_as;
    
    /* Setup new user context on current task's kernel stack */
    init_task_context(curr, (void*)entry, (void*)user_stack_top, true);
    struct cpu_context *frame = (struct cpu_context*)curr->context;
    frame->rax = 0;  // convention: execve returns 0 in the new program
    
    return 0;
}

/* sys_wait(pid, *status): wait for a child to exit */
int64_t sys_wait(uint64_t pid, uint64_t status_ptr, uint64_t arg2,
                 uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    int wp = (int)pid;
    int status = 0;
    struct task_struct *child = task_wait(wp, &status);
    if (child) {
        if (status_ptr) {
            *(int*)status_ptr = status;
        }
        return child->pid;
    }
    return -1;
}

/* sys_write(fd, buf, count): simple console output for stdout/stderr */
int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                  uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    kprintf("sys_write");
    int fd_num = (int)fd;
    const char *data = (const char*)buf;
    size_t len = (size_t)count;
    kprintf("fd_num: %llu\n", fd_num);
    if (fd_num == 1 || fd_num == 2) {
        char str[256];
        size_t n = (len < sizeof(str)-1) ? len : sizeof(str)-1;
        memcpy(str, data, n);
        str[n] = '\0';
        kprintf("[%s]", str);
        kprintf("[%llu]", len);
        return (int64_t)len;
    }
    return -1;
}


static inline void wrmsr64(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr"
                      :
                      : "c"(msr), "a"(lo), "d"(hi));
}


#define KERNEL_CS 0x08
#define USER_CS   0x2B          /* user code selector, DPL=3 */
#define FMASK     0x200         /* mask IF while in kernel   */

void syscall_init(void)
{
    uint64_t star  = ((uint64_t)USER_CS   << 48) |
                     ((uint64_t)KERNEL_CS << 32);   /* user CS, kernel CS */

    wrmsr64(0xC0000081, star);                  /* IA32_STAR  */
    wrmsr64(0xC0000082, (uint64_t)syscall_entry); /* IA32_LSTAR */
    wrmsr64(0xC0000084, FMASK);                 /* IA32_FMASK */

    /* turn on SYSCALL/SYSRET in EFER */
    uint64_t efer;
    __asm__ volatile ("rdmsr" : "=A"(efer) : "c"(0xC0000080));
    efer |= 1;                                  /* set SCE bit */
    wrmsr64(0xC0000080, efer);
    
}
