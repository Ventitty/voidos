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
        /* Se marquer bloqué PENDANT qu'on tient encore le verrou : c'est le
         * seul moyen de ne pas perdre un réveil. En relâchant d'abord, un
         * sem_post() concurrent pouvait nous retirer de la file et appeler
         * scheduler_unblock() sur une tâche pas encore marquée bloquée --
         * sans effet. On se bloquait juste après, pour toujours. Même
         * ordre que cond_wait(). */
        scheduler_mark_blocked_self();
        spinlock_release(&s->lock);

        /* Attente effective, hors verrou. */
        scheduler_yield_blocked(me);
    }
}

void sem_post(semaphore_t *s) {
    spinlock_acquire(&s->lock);

    /* Le compteur est incrémenté DANS TOUS LES CAS, y compris quand on
     * réveille un dormeur. L'ancienne version lui "transmettait" le jeton
     * sans toucher au compteur, mais la tâche réveillée repasse par le
     * début de sem_wait() et revérifie le compteur : elle le trouvait à
     * zéro et se rendormait aussitôt, définitivement. Le jeton était
     * perdu. Contrepartie : une tâche tierce peut consommer le jeton avant
     * la réveillée (pas de FIFO strict), mais aucun jeton ne se perd. */
    s->count++;

    int wake = -1;
    if (s->waitq_count > 0) {
        wake = s->waitq[s->waitq_head];
        s->waitq_head = (s->waitq_head + 1) % MAX_TASKS;
        s->waitq_count--;
    }

    spinlock_release(&s->lock);

    if (wake >= 0) {
        scheduler_unblock(wake);
    }
}
