#include <workqueue.h>
#include <8250.h>

static struct work_struct *work_head = 0;
static struct work_struct *work_tail = 0;

void schedule_work(struct work_struct *work) {
    if (!work->func) {
        serial_port_write("schedule_work: work->func is NULL!\n");
        return;
    }
    if (work->pending)
        return;
    work->pending = 1;
    work->next = 0;
    if (!work_head) {
        work_head = work;
        work_tail = work;
    } else {
        work_tail->next = work;
        work_tail = work;
    }
}

void process_workqueue(void) {
    if (work_head) {
        struct work_struct *work = work_head;
        work_head = work_head->next;
        if (!work_head)
            work_tail = 0;
        work->pending = 0;
        if (work->func)
            work->func(work->data);
        else
            serial_port_write("process_workqueue: work->func is NULL!\n");
    }
}
