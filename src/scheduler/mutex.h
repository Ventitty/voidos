#ifndef MUTEX_H
#define MUTEX_H

#include "src/types.h"
#include "src/scheduler/spinlock.h"
#include "src/scheduler/scheduler.h"

typedef struct {
    spinlock_t lock;
    volatile int owner_task_id;
    wait_queue_t waiters;
} mutex_t;

#define MUTEX_INIT { SPINLOCK_INIT, -1, WAIT_QUEUE_INIT }

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
int mutex_trylock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif /* MUTEX_H */
