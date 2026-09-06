#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "src/types.h"

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

#endif /* SPINLOCK_H */
