#ifndef EVENT_GROUP_H
#define EVENT_GROUP_H

#include "src/types.h"
#include "src/scheduler/spinlock.h"
#include "src/scheduler/scheduler.h"
#include "arch/xtensa_lx6/xtensa.h"

typedef struct {
    spinlock_t lock;
    volatile uint32_t bits;

    struct {
        uint8_t  active;
        uint32_t mask;
        uint8_t  wait_all;
        uint8_t  clear_on_exit;
    } waiting[MAX_TASKS];
} event_group_t;

void event_group_init(event_group_t *eg);
uint32_t event_group_set_bits(event_group_t *eg, uint32_t bits_to_set);
uint32_t event_group_clear_bits(event_group_t *eg, uint32_t bits_to_clear);
uint32_t event_group_get_bits(event_group_t *eg);
uint32_t event_group_wait_bits(event_group_t *eg, uint32_t mask, int clear_on_exit, int wait_for_all);

#endif /* EVENT_GROUP_H */
