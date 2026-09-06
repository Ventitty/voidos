.section .iram0.text, "ax"
.global _AppCpuResetVector
.type _AppCpuResetVector, @function
.align 4

.extern _stack_top_cpu1
.extern app_cpu_main

_AppCpuResetVector:
    movi    a0, 0
    wsr.intenable a0
    rsync

    movi    sp, _stack_top_cpu1

    call0   app_cpu_main

1:  j 1b

    .size _AppCpuResetVector, . - _AppCpuResetVector
