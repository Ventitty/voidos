.section .iram0.entry, "ax"
.global _ResetVector
.type _ResetVector, @function
.align 4

.extern _bss_start
.extern _bss_end
.extern _heap_start
.extern _heap_end
.extern _stack_top

_ResetVector:
    movi    a0, 0
    wsr.intenable a0
    rsync

    movi    sp, _stack_top

    movi    a2, _bss_start
    movi    a3, _bss_end
    movi    a4, 0

.L_zero_bss_loop:
    bgeu    a2, a3, .L_zero_bss_done
    s32i    a4, a2, 0
    addi    a2, a2, 4
    j       .L_zero_bss_loop

.L_zero_bss_done:

    movi    a2, _heap_start
    movi    a3, _heap_end

    call0   kernel_main

1:  j 1b
