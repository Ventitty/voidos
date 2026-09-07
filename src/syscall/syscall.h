#ifndef SYSCALL_H
#define SYSCALL_H

#include "src/types.h"

#define SYS_YIELD 0   /* cède volontairement le CPU (pas d'argument)          */
#define SYS_PRINT 1   /* a3 = const char* (chaîne terminée par '\0')          */
#define SYS_EXIT  2   /* termine la tâche appelante (pas d'argument)          */

static inline uint32_t syscall_invoke(uint32_t num, uint32_t a0, uint32_t a1, uint32_t a2) {
    register uint32_t r2 __asm__("a2") = num;
    register uint32_t r3 __asm__("a3") = a0;
    register uint32_t r4 __asm__("a4") = a1;
    register uint32_t r5 __asm__("a5") = a2;

    __asm__ volatile (
        "syscall\n"
        : "+r"(r2)
        : "r"(r3), "r"(r4), "r"(r5)
        : "memory"
    );

    return r2;
}

static inline void sys_yield(void) {
    syscall_invoke(SYS_YIELD, 0, 0, 0);
}

static inline void sys_print(const char *str) {
    syscall_invoke(SYS_PRINT, (uint32_t)str, 0, 0);
}

static inline void sys_exit(void) {
    syscall_invoke(SYS_EXIT, 0, 0, 0);
    while (1) { }
}

#endif /* SYSCALL_H */
