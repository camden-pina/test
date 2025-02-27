#ifndef KERNEL_PROC_H
#define KERNEL_PROC_H

#include <stdint.h>
#include <stddef.h>
#include <mutex.h>
#include <queue.h>
#include <ref.h>
#include <kernel.h>
#include <str.h>

#define __NEED_struct_timeval

struct lockqueue;
struct runqueue;

struct thread;

typedef struct proc {
  uint32_t flags;
  struct pcreds *creds;

  enum proc_state {
    PRS_EMPTY,
    PRS_ACTIVE,
    PRS_ZOMBIE,
    PRS_EXITED,
  } state;

  mtx_t lock;
  mtx_t statlock;

  str_t binpath;

  uintptr_t brk_start;
  uintptr_t brk_end;
  uintptr_t brk_max;

  str_t name;

  int exit_code;

  volatile uint32_t num_eiting;
  volatile uint32_t num_exited;
  uint32_t num_threads;
  LIST_HEAD(struct thread) threads;
} proc_t;

#define PRF_LEADER 0x1   // process is group leader
#define   PRF_IS_LEADER(p) ((p)->flags & PRF_LEADER)
#define PRF_HASRUN 0x2   // process has run at least once
#define   PRF_HAS_RUN(p) ((p)->flags & PRF_HASRUN)

#define PRS_IS_EMPTY(p) ((p)->state == PRS_EMPTY)
#define PRS_IS_ALIVE(p) ((p)->state == PRS_ACTIVE)
#define PRS_IS_ZOMBIE(p) ((p)->state == PRS_ZOMBIE)
#define PRS_IS_EXITED(p) ((p)->state == PRS_EXITED)

#define pr_main_thread(p) LIST_FIRST(&(p)->threads)
#define pr_lock_assert(p, what) __type_checked(proc_t*, p, mtx_assert(&(p)->lock, what))
#define pr_lock(p) __type_checked(proc_t*, p, mtx_lock(&(p)->lock))
#define pr_unlock(p) __type_checked(proc_t*, p, mtx_unlock(&(p)->lock))


struct pcreds {
  uid_t uid;  // user id
  uid_t euid; // effective user id
  gid_t gid;  // group id
  gid_t egid; // effective group id
  _refcount;
};

typedef struct thread {
    uint32_t flags;
    mtx_t lock;
    struct proc *proc;

    struct lockqueue *own_lockq;          // thread owned lockq

    uintptr_t kstack_base;
    size_t kstack_size;


  enum thread_state {
    TDS_EMPTY,    // thread is being set up
    TDS_READY,    // thread is on a runqueue
    TDS_RUNNING,  // thread is running on a cpu
    TDS_BLOCKED,  // thread is on a lockqueue
    TDS_WAITING,  // thread is on a waitqueue
    TDS_EXITED,   // thread has exited
  } state;

    // start complete
    int cpu_id;                 // last cpu thread ran on
    volatile uint32_t flags2;   // private flags
    uint16_t : 16;
    uint8_t pri_base;           // base priority
    uint8_t piority;            // current priority
    // end complete

    str_t name;
    uint64_t runtime;
    uint64_t blocktime;
    // struct timeval start_time;            // thread start time

    int lock_count;                       // number of normal mutexes held
    int spin_count;                       // number of spin mutexes held
    int crit_level;                       // critical section level

    int errno;

    struct runqueue *runq;                // runqueue (if ready)

    LIST_ENTRY(struct thread) plist;      // process thread list entry
    LIST_ENTRY(struct thread) rqlist;     // runq list entry
    LIST_ENTRY(struct thread) lqlist;     // lockq list entry
    LIST_ENTRY(struct thread) wqlist;     // waitq list entry
} thread_t;

// thread flags
#define TDF_KTHREAD     0x00000001  // kernel thread
#define   TDF_IS_KTHREAD(td) ((td)->flags & TDF_KTHREAD)
#define TDF_ITHREAD     0x00000002  // interrupt thread
#define   TDF_IS_ITHREAD(td) ((td)->flags & TDF_ITHREAD)
#define TDF_IDLE        0x00000004  // per-cpu idle thread
#define   TDF_IS_IDLE(td) ((td)->flags & TDF_IDLE)

// private thread flags
#define TDF2_STOPPING   0x00000001  // thread has been marked for exit
#define   TDF2_IS_STOPPING(td) ((td)->flags2 & TDF2_STOPPING)
#define TDF2_FIRSTTIME  0x00000002  // thread has not yet run
#define   TDF2_IS_FIRSTTIME(td) ((td)->flags2 & TDF2_FIRSTTIME)
#define TDF2_AFFINITY   0x00000004  // thread has cpu affinity
#define   TDF2_HAS_AFFINITY(td) ((td)->flags2 & TDF2_AFFINITY)
#define TDF2_INTRP      0x00000008  // thread was interrupted
#define   TDF2_WAS_INTRP(td) ((td)->flags2 & TDF2_INTRP)

#define TDS_IS_EMPTY(td) ((td)->state == TDS_EMPTY)
#define TDS_IS_READY(td) ((td)->state == TDS_READY)
#define TDS_IS_RUNNING(td) ((td)->state == TDS_RUNNING)
#define TDS_IS_BLOCKED(td) ((td)->state == TDS_BLOCKED)
#define TDS_IS_WAITING(td) ((td)->state == TDS_WAITING)
#define TDS_IS_EXITED(td) ((td)->state == TDS_EXITED)

#define TD_CPU_ID()

#define TD_SET_STATE(td, s) ((td)->state = (s))
#define TD_SET_CPU(td, id) ((td)->)

#define td_lock_assert(td, what) __type_checked(thread_t*, td, mtx_assert(&(td)->lock, what))
#define td_lock(td) _thread_lock(td, __FILE__, __LINE__)
#define td_unlock(td) _thread_unlock(td, __FILE__, __LINE__)

thread_t *thread_alloc(uint32_t flags, size_t kstack_size);

void critical_enter();
void critical_exit();

#endif