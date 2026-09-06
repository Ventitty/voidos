#include "src/scheduler/cond.h"

void cond_init(cond_t *c) {
    spinlock_acquire(&c->lock);
    c->waitq_head = 0;
    c->waitq_tail = 0;
    c->waitq_count = 0;
    spinlock_release(&c->lock);
}

void cond_wait(cond_t *c, mutex_t *m) {
    spinlock_acquire(&c->lock);

    int me = scheduler_current_task_id();
    if (me >= 0) {
        if (c->waitq_count < MAX_TASKS) {
            c->waitq[c->waitq_tail] = me;
            c->waitq_tail = (c->waitq_tail + 1) % MAX_TASKS;
            c->waitq_count++;
        }

        scheduler_mark_blocked_self();
    }

    spinlock_release(&c->lock);
    mutex_unlock(m);

    if (me >= 0) {
        scheduler_yield_blocked(me);
    }

    mutex_lock(m);
}

void cond_signal(cond_t *c) {
    spinlock_acquire(&c->lock);

    int wake = -1;
    if (c->waitq_count > 0) {
        wake = c->waitq[c->waitq_head];
        c->waitq_head = (c->waitq_head + 1) % MAX_TASKS;
        c->waitq_count--;
    }

    spinlock_release(&c->lock);

    if (wake >= 0) {
        scheduler_unblock(wake);
    }
}

void cond_broadcast(cond_t *c) {
    spinlock_acquire(&c->lock);

    int to_wake[MAX_TASKS];
    int count = 0;
    while (c->waitq_count > 0) {
        to_wake[count++] = c->waitq[c->waitq_head];
        c->waitq_head = (c->waitq_head + 1) % MAX_TASKS;
        c->waitq_count--;
    }

    spinlock_release(&c->lock);

    for (int i = 0; i < count; i++) {
        scheduler_unblock(to_wake[i]);
    }
}
