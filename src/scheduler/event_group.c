#include "src/scheduler/event_group.h"

static inline int condition_met(uint32_t bits, uint32_t mask, int wait_for_all) {
    return wait_for_all ? ((bits & mask) == mask) : ((bits & mask) != 0);
}

void event_group_init(event_group_t *eg) {
    spinlock_acquire(&eg->lock);
    eg->bits = 0;
    wait_queue_init(&eg->waiters);
    spinlock_release(&eg->lock);
}

uint32_t event_group_set_bits(event_group_t *eg, uint32_t bits_to_set) {
    spinlock_acquire(&eg->lock);

    eg->bits |= bits_to_set;
    uint32_t result = eg->bits;

    /* Parcourt la file et retire chaque tâche dont la condition est
     * remplie ; les autres restent en attente, dans le même ordre. */
    task_t *prev = NULL;
    task_t *t = eg->waiters.head;
    while (t != NULL) {
        task_t *next = t->wait_next;

        if (condition_met(eg->bits, t->wait_mask, t->wait_all)) {
            if (prev) prev->wait_next = next;
            else      eg->waiters.head = next;
            if (eg->waiters.tail == t) eg->waiters.tail = prev;

            t->wait_next = NULL;
            scheduler_unblock(t);
        } else {
            prev = t;
        }
        t = next;
    }

    spinlock_release(&eg->lock);
    return result;
}

uint32_t event_group_clear_bits(event_group_t *eg, uint32_t bits_to_clear) {
    spinlock_acquire(&eg->lock);
    uint32_t before = eg->bits;
    eg->bits &= ~bits_to_clear;
    spinlock_release(&eg->lock);
    return before;
}

uint32_t event_group_get_bits(event_group_t *eg) {
    spinlock_acquire(&eg->lock);
    uint32_t b = eg->bits;
    spinlock_release(&eg->lock);
    return b;
}

uint32_t event_group_wait_bits(event_group_t *eg, uint32_t mask,
                               int clear_on_exit, int wait_for_all) {
    while (1) {
        spinlock_acquire(&eg->lock);

        if (condition_met(eg->bits, mask, wait_for_all)) {
            uint32_t result = eg->bits;
            if (clear_on_exit) {
                eg->bits &= ~mask;
            }
            spinlock_release(&eg->lock);
            return result;
        }

        task_t *me = scheduler_current_task();
        if (me == NULL) {
            spinlock_release(&eg->lock);
            continue;
        }

        me->wait_mask = mask;
        me->wait_all  = (uint8_t)wait_for_all;
        wait_queue_push(&eg->waiters, me);

        scheduler_mark_blocked_self();

        spinlock_release(&eg->lock);

        scheduler_yield_blocked(me);
    }
                               }
