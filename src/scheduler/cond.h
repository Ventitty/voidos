#ifndef COND_H
#define COND_H

#include "src/types.h"
#include "src/scheduler/spinlock.h"
#include "src/scheduler/scheduler.h"
#include "src/scheduler/mutex.h"

typedef struct {
    spinlock_t lock;
    wait_queue_t waiters;
} cond_t;

#define COND_INIT { SPINLOCK_INIT, WAIT_QUEUE_INIT }

void cond_init(cond_t *c);
void cond_wait(cond_t *c, mutex_t *m);
void cond_signal(cond_t *c);
void cond_broadcast(cond_t *c);

#endif /* COND_H */
