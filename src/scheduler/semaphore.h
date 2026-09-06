#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "src/types.h"
#include "src/scheduler/spinlock.h"
#include "src/scheduler/scheduler.h"
#include "arch/xtensa_lx6/xtensa.h"

typedef struct {
    spinlock_t lock;
    volatile int count;
    int waitq[MAX_TASKS];
    int waitq_head;
    int waitq_tail;
    int waitq_count;
} semaphore_t;

#define SEMAPHORE_INIT(n) { SPINLOCK_INIT, (n), {0}, 0, 0, 0 }

void sem_init(semaphore_t *s, int initial_count);
void sem_wait(semaphore_t *s);
int sem_trywait(semaphore_t *s);
void sem_post(semaphore_t *s);

#endif /* SEMAPHORE_H */
