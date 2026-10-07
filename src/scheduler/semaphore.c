#include "src/scheduler/semaphore.h"

void sem_init(semaphore_t *s, int initial_count) {
    spinlock_acquire(&s->lock);
    s->count = initial_count;
    wait_queue_init(&s->waiters);
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

        task_t *me = scheduler_current_task();
        if (me == NULL) {
            spinlock_release(&s->lock);
            continue;
        }

        wait_queue_push(&s->waiters, me);
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

    task_t *wake = wait_queue_pop(&s->waiters);

    spinlock_release(&s->lock);

    scheduler_unblock(wake);   /* sans effet si NULL */
}
