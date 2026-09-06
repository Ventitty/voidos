#include "src/scheduler/scheduler.h"

static task_t tasks[MAX_TASKS];
static spinlock_t tasks_lock = SPINLOCK_INIT;

static volatile int current_task_idx[2] = { -1, -1 };

static int global_cursor = -1;

uint32_t get_core_id(void) {
    uint32_t prid;
    __asm__ volatile ("rsr.prid %0" : "=r"(prid));
    return (prid >> 13) & 1u;
}

static void task_exit(void) {
    uint32_t core = get_core_id();

    spinlock_acquire(&tasks_lock);
    int cur = current_task_idx[core];
    if (cur >= 0) {
        tasks[cur].state = TASK_UNUSED;
    }
    spinlock_release(&tasks_lock);

    while (1) {
        __asm__ volatile ("waiti 0");
    }
}

void scheduler_init(void) {
    spinlock_acquire(&tasks_lock);
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
        tasks[i].sp = 0;
        tasks[i].stack = 0;
        tasks[i].pinned_core = TASK_ANY_CORE;
        tasks[i].running_on = TASK_ANY_CORE;
    }
    global_cursor = -1;
    spinlock_release(&tasks_lock);

    current_task_idx[0] = -1;
    current_task_idx[1] = -1;
}

static int task_create_common(void (*entry)(void), uint8_t pinned_core) {
    spinlock_acquire(&tasks_lock);

    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) { slot = i; break; }
    }
    if (slot < 0) {
        spinlock_release(&tasks_lock);
        return -1;
    }

    tasks[slot].state = TASK_BLOCKED;
    spinlock_release(&tasks_lock);

    void *stack = nmap(TASK_STACK_SIZE);
    if (stack == NULL) {
        tasks[slot].state = TASK_UNUSED;
        return -1;
    }

    uint8_t *stack_top = (uint8_t *)stack + TASK_STACK_SIZE;
    cpu_context_t *ctx = (cpu_context_t *)(stack_top - sizeof(cpu_context_t));

    for (uint32_t *p = (uint32_t *)ctx; p < (uint32_t *)stack_top; p++) *p = 0;

    ctx->sp_orig  = (uint32_t)stack_top;
    ctx->epc      = (uint32_t)entry;
    ctx->a0       = (uint32_t)task_exit;
    ctx->exccause = EXCCAUSE_LEVEL1_INTERRUPT;

    uint32_t ps;
    __asm__ volatile ("rsr %0, ps" : "=r"(ps));
    ctx->ps = ps;

    tasks[slot].sp          = (uint32_t *)ctx;
    tasks[slot].stack       = stack;
    tasks[slot].pinned_core = pinned_core;
    tasks[slot].running_on  = TASK_ANY_CORE;
    tasks[slot].id          = (uint32_t)slot;
    tasks[slot].state       = TASK_READY;

    return slot;
}

int task_create(void (*entry)(void)) {
    return task_create_common(entry, TASK_ANY_CORE);
}

int task_create_pinned(void (*entry)(void), uint8_t core) {
    if (core > 1) return -1;
    return task_create_common(entry, core);
}

static inline int task_eligible_for(const task_t *t, uint32_t core) {
    return t->pinned_core == TASK_ANY_CORE || t->pinned_core == core;
}

uint32_t *schedule_next_task(uint32_t *current_sp) {
    uint32_t core = get_core_id();

    spinlock_acquire(&tasks_lock);

    int cur = current_task_idx[core];
    if (cur >= 0) {
        tasks[cur].sp = current_sp;
        tasks[cur].running_on = TASK_ANY_CORE;
        if (tasks[cur].state == TASK_RUNNING) {
            tasks[cur].state = TASK_READY;
        }
    }

    int start = global_cursor;
    int next = -1;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (start + i + MAX_TASKS) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY && task_eligible_for(&tasks[idx], core)) {
            next = idx;
            break;
        }
    }

    if (next < 0) {
        if (cur >= 0 && tasks[cur].state == TASK_READY) {
            tasks[cur].state = TASK_RUNNING;
            tasks[cur].running_on = core;
            spinlock_release(&tasks_lock);
            return current_sp;
        }
        spinlock_release(&tasks_lock);
        current_task_idx[core] = -1;
        return current_sp;
    }

    tasks[next].state = TASK_RUNNING;
    tasks[next].running_on = core;
    current_task_idx[core] = next;
    global_cursor = next;
    uint32_t *new_sp = tasks[next].sp;

    spinlock_release(&tasks_lock);

    return new_sp;
}

int scheduler_current_task_id(void) {
    return current_task_idx[get_core_id()];
}

int scheduler_mark_blocked_self(void) {
    int me = scheduler_current_task_id();
    if (me < 0) return -1;

    spinlock_acquire(&tasks_lock);
    tasks[me].state = TASK_BLOCKED;
    spinlock_release(&tasks_lock);

    return me;
}

void scheduler_yield_blocked(int me) {
    if (me < 0) return;

    while (tasks[me].state == TASK_BLOCKED) {
        __asm__ volatile ("waiti 0");
    }
}

void scheduler_block_current(void) {
    int me = scheduler_mark_blocked_self();
    scheduler_yield_blocked(me);
}

void scheduler_unblock(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return;

    spinlock_acquire(&tasks_lock);
    if (tasks[task_id].state == TASK_BLOCKED) {
        tasks[task_id].state = TASK_READY;
    }
    spinlock_release(&tasks_lock);
}

void scheduler_start(void) {
    uint32_t core = get_core_id();

    interrupts_init_this_core();

    current_task_idx[core] = -1;

    interrupts_enable_line(TIMER0_IRQ_LINE);
    set_cpu_private_timer(0, TICK_CYCLES);
    interrupts_enable_global();

    while (1) {
        __asm__ volatile ("waiti 0");
    }
}
