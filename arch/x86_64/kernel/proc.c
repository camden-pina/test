#include <proc.h>
#include <cpu.h>
#include <kernel.h>
#include <atomic.h>
#include <percpu.h>
#include <mm_types.h>
#include <mutex.h>
#include <panic.h>

void critical_enter() {
    thread_t *td = curthread;
  
    // disable interrupts
    uint64_t rflags;
    temp_irq_save(rflags);
    if (__expect_false(td == NULL)) {
      // no thread yet, save flags
      PERCPU_SET_RFLAGS(rflags);
      return;
    }
  
    if (atomic_fetch_add(&td->crit_level, 1) == 0) {
      // first time entering a critical section, save flags
      PERCPU_SET_RFLAGS(rflags);
    }
  }

void critical_exit() {
    thread_t *td = curthread;
    if (__expect_false(td == NULL)) {
        uint64_t flags = PERCPU_RFLAGS;
        temp_irq_restore(flags);
        return;
    }
    if (atomic_fetch_sub(&td->crit_level, 1) == 1) {
        // last time exiting a critical section, restore flags
        uint64_t flags = PERCPU_RFLAGS;
        temp_irq_restore(flags);
    }
}

static inline void proc_do_add_thread(proc_t *proc, thread_t *td) {
  td->proc = proc;
  td->creds = getref(proc->creds);
  td->tid = (pid_t)proc->num_threads;
  LIST_ADD(&proc->threads, td, plist);
  proc->num_threads++;
}

void proc_add_thread(proc_t *proc, thread_t *td) {
  ASSERT(TDS_IS_EMPTY(td));
  ASSERT(td->proc == NULL);

  pr_lock(proc);
  ASSERT(PRS_IS_ALIVE(proc));

  proc_do_add_thread(proc, td);
  pr_unlock(proc);
}

thread_t *thread_alloc(uint32_t flags, size_t kstack_size) {
  ASSERT(kstack_size > 0 && is_aligned(kstack_size, PAGE_SIZE));
  uintptr_t kstack_base = vmap_pages(alloc_pages(SIZE_TO_PAGES(kstack_size)), 0, kstack_size, VM_RDWR|VM_STACK, "kstack");
  return thread_alloc_internal(flags, kstack_base, kstack_size);
}

thread_t *thread_alloc_idle() {
  // this is called once by each cpu during scheduler initialization
  thread_t *td = thread_alloc(TDF_KTHREAD|TDF_IDLE, SIZE_8KB);
  td->state = TDS_READY;
  td->name = str_fmt("idle thread [CPU#%d]", curcpu_id);
  td->tcb->rip = (uintptr_t) idle_thread_entry;

  mtx_spin_lock(&proc0_ap_lock);
  proc_do_add_thread(proc0, td);
  mtx_spin_unlock(&proc0_ap_lock);
  return td;
}

void thread_free_exited(thread_t **tdp) {
  thread_t *td = *tdp;
  ASSERT(TDS_IS_EXITED(td));
  td_lock_assert(td, MA_UNLOCKED);

  mtx_destroy(&td->lock);
  kfree(td->frame);
  td->frame = NULL;
  lock_claim_list_free(&td->wait_claims);
  lockq_free(&td->own_lockq);
  waitq_free(&td->own_waitq);
  cpuset_free(&td->cpuset);

  pcreds_release(&td->creds);
  // tcb_free(&td->tcb);
  todo();
  kfree(td);
  *tdp = NULL;
}
