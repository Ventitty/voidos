#include "src/scheduler/cond.h"

void cond_init(cond_t *c) {
    spinlock_acquire(&c->lock);
    wait_queue_init(&c->waiters);
    spinlock_release(&c->lock);
}

void cond_wait(cond_t *c, mutex_t *m) {
    spinlock_acquire(&c->lock);

    task_t *me = scheduler_current_task();
    if (me != NULL) {
        wait_queue_push(&c->waiters, me);
        scheduler_mark_blocked_self();
    }

    spinlock_release(&c->lock);
    mutex_unlock(m);

    scheduler_yield_blocked(me);   /* sans effet si NULL */

    mutex_lock(m);
}

void cond_signal(cond_t *c) {
    spinlock_acquire(&c->lock);
    task_t *wake = wait_queue_pop(&c->waiters);
    spinlock_release(&c->lock);

    scheduler_unblock(wake);   /* sans effet si NULL */
}

void cond_broadcast(cond_t *c) {
    spinlock_acquire(&c->lock);
    task_t *t = wait_queue_take_all(&c->waiters);
    spinlock_release(&c->lock);

    while (t != NULL) {
        task_t *next = t->wait_next;   /* lu AVANT le réveil (voir scheduler.h) */
        t->wait_next = NULL;
        scheduler_unblock(t);
        t = next;
    }
}
