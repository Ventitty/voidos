#include "src/scheduler/mutex.h"

void mutex_init(mutex_t *m) {
    spinlock_acquire(&m->lock);
    m->owner_task_id = -1;
    m->waitq_head = 0;
    m->waitq_tail = 0;
    m->waitq_count = 0;
    spinlock_release(&m->lock);
}

int mutex_trylock(mutex_t *m) {
    int got = 0;

    spinlock_acquire(&m->lock);
    if (m->owner_task_id < 0) {
        m->owner_task_id = scheduler_current_task_id();
        got = 1;
    }
    spinlock_release(&m->lock);

    return got;
}

void mutex_lock(mutex_t *m) {
    while (1) {
        spinlock_acquire(&m->lock);

        if (m->owner_task_id < 0) {
            m->owner_task_id = scheduler_current_task_id();
            spinlock_release(&m->lock);
            return;
        }

        int me = scheduler_current_task_id();
        if (me < 0) {
            spinlock_release(&m->lock);
            continue;
        }

        if (m->waitq_count < MAX_TASKS) {
            m->waitq[m->waitq_tail] = me;
            m->waitq_tail = (m->waitq_tail + 1) % MAX_TASKS;
            m->waitq_count++;
        }
        /* Se marquer bloqué PENDANT qu'on tient encore le verrou : c'est le
         * seul moyen de ne pas perdre un réveil. En relâchant d'abord, un
         * mutex_unlock() concurrent pouvait nous retirer de la file et appeler
         * scheduler_unblock() sur une tâche pas encore marquée bloquée --
         * sans effet. On se bloquait juste après, pour toujours. Même
         * ordre que cond_wait(). */
        scheduler_mark_blocked_self();
        spinlock_release(&m->lock);

        /* Attente effective, hors verrou. */
        scheduler_yield_blocked(me);
    }
}

void mutex_unlock(mutex_t *m) {
    spinlock_acquire(&m->lock);

    m->owner_task_id = -1;

    int wake = -1;
    if (m->waitq_count > 0) {
        wake = m->waitq[m->waitq_head];
        m->waitq_head = (m->waitq_head + 1) % MAX_TASKS;
        m->waitq_count--;
    }

    spinlock_release(&m->lock);

    if (wake >= 0) {
        scheduler_unblock(wake);
    }
}
