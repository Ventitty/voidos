#ifndef EVENT_GROUP_H
#define EVENT_GROUP_H

#include "src/types.h"
#include "src/scheduler/spinlock.h"
#include "src/scheduler/scheduler.h"

typedef struct {
    spinlock_t lock;
    volatile uint32_t bits;
    /* Tâches en attente ; chacune porte son masque dans wait_mask/wait_all. */
    wait_queue_t waiters;
} event_group_t;

#define EVENT_GROUP_INIT { SPINLOCK_INIT, 0, WAIT_QUEUE_INIT }

void event_group_init(event_group_t *eg);
uint32_t event_group_set_bits(event_group_t *eg, uint32_t bits_to_set);
uint32_t event_group_clear_bits(event_group_t *eg, uint32_t bits_to_clear);
uint32_t event_group_get_bits(event_group_t *eg);
uint32_t event_group_wait_bits(event_group_t *eg, uint32_t mask, int clear_on_exit, int wait_for_all);

#endif /* EVENT_GROUP_H */
