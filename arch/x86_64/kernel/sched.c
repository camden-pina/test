#include <sched.h>
#include <proc.h>
#include <panic.h>
#include <printf.h>
#include <atomic.h>
#include <percpu.h>
#include <cpu.h>
#include <heap.h>

#define ASSERT(x) kassert(x)
#define DPRINTF(x, ...) kprintf("sched: " x, ##__VA_ARGS__)

#define NRUNQS 64

/*
 * A scheduler on a cpu.
 */
typedef struct sched {
    uint64_t id;                    // scheduler id
    volatile uint64_t readymask;    // runqueues with ready threads (bitmap)
    struct runqueue queues[NRUNQS]; // runqueues (indexed by td->priority/4)
    thread_t *idle;                 // idle thread
    uint64_t last_switch;           // last time a thread switch occured (ns)
  } sched_t;
  static sched_t *cpu_scheds[MAX_CPUS];

void sched_init() {
    sched_t *sched = kmallocz(sizeof(sched_t));
    sched->id = curcpu_id;
    sched->idle = thread_alloc_idle();
    for (int i = 0; i < NRUNQS; i++) {
      runq_init(&sched->queues[i]);
    }
    cpu_scheds[curcpu_id] = sched;
  
    set_cursched(sched);
    if (curthread == NULL) {
      // have the APs switch to their idle threads
      thread_t *td = sched->idle;
      TD_SET_STATE(td, TDS_RUNNING);
      td->start_time = clock_micro_time();
  
      sched_do_switch(NULL, td);
      unreachable;
    }
  }

void sched_again(sched_reason_t reason) {
    thread_t *oldtd = curthread;
    if (mtx_owner(&oldtd->lock) == NULL) {
      // lock the thread if it hasnt been already
      mtx_lock(&oldtd->lock);
    }
    td_lock_assert(oldtd, MA_NOTRECURSED);
  
    // update thread state+stats
    switch (reason) {
      case SCHED_UPDATED:
        break;
      case SCHED_YIELDED:
        TD_SET_STATE(oldtd, TDS_READY);
        break;
      case SCHED_BLOCKED:
        TD_SET_STATE(oldtd, TDS_BLOCKED);
        break;
      case SCHED_SLEEPING:
        TD_SET_STATE(oldtd, TDS_WAITING);
        break;
      case SCHED_PREEMPTED:
        TD_SET_STATE(oldtd, TDS_READY);
        break;
      case SCHED_EXITED:
        ASSERT(TDF2_IS_STOPPING(oldtd));
        TD_SET_STATE(oldtd, TDS_EXITED);
        break;
      default:
        unreachable;
    }
  
    thread_t *newtd = sched_next_thread();
    td_lock_assert(newtd, MA_OWNED);
    TD_SET_STATE(newtd, TDS_RUNNING);
  
    if (TDF2_IS_FIRSTTIME(newtd)) {
      newtd->start_time = clock_micro_time();
    }
  
    td_unlock(newtd);
    sched_do_switch(oldtd, newtd);
  }
