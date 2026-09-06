#include "src/scheduler/semaphore.h"

void sem_init(semaphore_t *s, int initial_count) {
    spinlock_acquire(&s->lock);
    s->count = initial_count;
    s->waitq_head = 0;
    s->waitq_tail = 0;
    s->waitq_count = 0;
    spinlock_release(&s->lock);
}

int sem_trywait(semaphore_t *s) {
    int got = 0;

    spinlock_acquire(&s->lock);
    if (s->count > 0) {
        s->count--;
        got = 1;
    }
    spinlock_release(&s->lock);

    return got;
}

void sem_wait(semaphore_t *s) {
    while (1) {
        spinlock_acquire(&s->lock);

        if (s->count > 0) {
            s->count--;
            spinlock_release(&s->lock);
            return;
        }

        int me = scheduler_current_task_id();
        if (me < 0) {
            spinlock_release(&s->lock);
            continue;
        }

        if (s->waitq_count < MAX_TASKS) {
            s->waitq[s->waitq_tail] = me;
            s->waitq_tail = (s->waitq_tail + 1) % MAX_TASKS;
            s->waitq_count++;
        }
        spinlock_release(&s->lock);

        scheduler_block_current();
    }
}

void sem_post(semaphore_t *s) {
    spinlock_acquire(&s->lock);

    int wake = -1;
    if (s->waitq_count > 0) {
        wake = s->waitq[s->waitq_head];
        s->waitq_head = (s->waitq_head + 1) % MAX_TASKS;
        s->waitq_count--;
    } else {
        s->count++;
    }

    spinlock_release(&s->lock);

    if (wake >= 0) {
        scheduler_unblock(wake);
    }
}
