#include "src/scheduler/spinlock.h"

void spinlock_acquire(spinlock_t *lock) {
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
}

void spinlock_release(spinlock_t *lock) {
    __asm__ volatile ("" ::: "memory");
    lock->locked = 0;
}
