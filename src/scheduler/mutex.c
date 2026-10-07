#include "src/scheduler/mutex.h"

void mutex_init(mutex_t *m) {
    spinlock_acquire(&m->lock);
    m->owner_task_id = -1;
    wait_queue_init(&m->waiters);
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

        task_t *me = scheduler_current_task();
        if (me == NULL) {
            spinlock_release(&m->lock);
            continue;
        }

        wait_queue_push(&m->waiters, me);
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

    task_t *wake = wait_queue_pop(&m->waiters);

    spinlock_release(&m->lock);

    scheduler_unblock(wake);   /* sans effet si NULL */
}
