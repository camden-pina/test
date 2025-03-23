/* workqueue.h */
#ifndef WORKQUEUE_H
#define WORKQUEUE_H

struct work_struct {
    void (*func)(void *);
    void *data;
    struct work_struct *next;
    int pending;  // New flag to indicate if work is already enqueued.
};

void schedule_work(struct work_struct *work);
void process_workqueue(void);

#endif
