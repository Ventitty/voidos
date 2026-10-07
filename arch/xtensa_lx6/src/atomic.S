/* Opérations atomiques Xtensa LX6 (ABI call0 : arguments dans a2..a7,
 * valeur de retour dans a2, adresse de retour dans a0). */

.section .iram0.text, "ax"

/* uint32_t cpu_atomic_cas(volatile uint32_t *ptr,   -> a2
 *                         uint32_t expected,        -> a3
 *                         uint32_t desired);        -> a4
 *
 * s32c1i compare *ptr à SCOMPARE1 ; s'ils sont égaux, écrit a4 dans *ptr.
 * Dans tous les cas a4 reçoit l'ancienne valeur de *ptr. */
.global cpu_atomic_cas
.type   cpu_atomic_cas, @function
.align  4
cpu_atomic_cas:
    wsr     a3, scompare1
    s32c1i  a4, a2, 0
    mov     a2, a4
    ret
.size cpu_atomic_cas, . - cpu_atomic_cas
