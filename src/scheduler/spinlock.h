#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "src/types.h"

#define SPINLOCK_INIT { 0, 0 }
#define SPINLOCK_INTLEVEL 3u
#define PS_INTLEVEL_MASK  0xFu

typedef struct {
    volatile uint32_t locked;
    uint32_t saved_ps;
} spinlock_t;

void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

#endif /* SPINLOCK_H */
