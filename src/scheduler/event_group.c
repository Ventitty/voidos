#include "src/scheduler/event_group.h"

static inline int condition_met(uint32_t bits, uint32_t mask, int wait_for_all) {
    return wait_for_all ? ((bits & mask) == mask) : ((bits & mask) != 0);
}

void event_group_init(event_group_t *eg) {
    spinlock_acquire(&eg->lock);
    eg->bits = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        eg->waiting[i].active = 0;
    }
    spinlock_release(&eg->lock);
}

uint32_t event_group_set_bits(event_group_t *eg, uint32_t bits_to_set) {
    spinlock_acquire(&eg->lock);

    eg->bits |= bits_to_set;
    uint32_t result = eg->bits;

    for (int i = 0; i < MAX_TASKS; i++) {
        if (!eg->waiting[i].active) continue;
        if (condition_met(eg->bits, eg->waiting[i].mask, eg->waiting[i].wait_all)) {
            eg->waiting[i].active = 0;
            scheduler_unblock(i);
        }
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

        int me = scheduler_current_task_id();
        if (me < 0) {
            spinlock_release(&eg->lock);
            continue;
        }

        eg->waiting[me].active        = 1;
        eg->waiting[me].mask          = mask;
        eg->waiting[me].wait_all      = (uint8_t)wait_for_all;
        eg->waiting[me].clear_on_exit = (uint8_t)clear_on_exit;

        scheduler_mark_blocked_self();

        spinlock_release(&eg->lock);

        scheduler_yield_blocked(me);
    }
}
