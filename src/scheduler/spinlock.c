#include "src/scheduler/spinlock.h"

void spinlock_acquire(spinlock_t *lock) {
    uint32_t ps = cpu_read_ps();
    if ((ps & PS_INTLEVEL_MASK) < SPINLOCK_INTLEVEL) {
        ps = cpu_irq_mask_level3();
    }

    while (cpu_atomic_cas(&lock->locked, 0, 1) != 0) { }

    lock->saved_ps = ps;
}

void spinlock_release(spinlock_t *lock) {
    uint32_t ps = lock->saved_ps;

    cpu_mem_barrier();
    lock->locked = 0;

    cpu_write_ps(ps);
}
