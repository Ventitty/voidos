#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "src/types.h"
#include "src/interrupts/interrupts.h"
#include "src/memory_manager/memory.h"
#include "src/scheduler/spinlock.h"

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
} task_state_t;

typedef struct task {
    uint32_t     *sp;
    void         *stack;
    task_state_t  state;
    uint8_t       pinned_core;
    uint8_t       running_on;
    uint32_t      id;
} task_t;

void scheduler_init(void);
int task_create(void (*entry)(void));
int task_create_pinned(void (*entry)(void), uint8_t core);

int task_create_user(void (*entry)(void));
int task_create_user_pinned(void (*entry)(void), uint8_t core);

void scheduler_start(void);
uint32_t *schedule_next_task(uint32_t *current_sp);

uint32_t *scheduler_terminate_current(uint32_t *sp);
uint32_t get_core_id(void);
int  scheduler_current_task_id(void);
void scheduler_block_current(void);
void scheduler_unblock(int task_id);
int  scheduler_mark_blocked_self(void);
void scheduler_yield_blocked(int me);

#endif /* SCHEDULER_H */
