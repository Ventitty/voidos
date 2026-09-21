#include "src/scheduler/spinlock.h"

void spinlock_acquire(spinlock_t *lock) {
    uint32_t ps;

    __asm__ volatile ("rsr %0, ps" : "=r"(ps));
    if ((ps & PS_INTLEVEL_MASK) < SPINLOCK_INTLEVEL) {
        __asm__ volatile ("rsil %0, 3" : "=r"(ps) :: "memory");
    }

    uint32_t want = 1;
    uint32_t got;
    do {
        __asm__ volatile (
            "movi   a3, 0\n"
            "wsr    a3, scompare1\n"
            "mov    %0, %1\n"
            "s32c1i %0, %2, 0\n"
            : "=&r"(got)
            : "r"(want), "r"(&lock->locked)
            : "a3", "memory"
        );
    } while (got != 0);

    lock->saved_ps = ps;
}

void spinlock_release(spinlock_t *lock) {
    uint32_t ps = lock->saved_ps;

    __asm__ volatile ("memw" ::: "memory");
    lock->locked = 0;

    __asm__ volatile ("wsr %0, ps\n\trsync" :: "r"(ps) : "memory");
}
