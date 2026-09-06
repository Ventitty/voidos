#include "src/smp/smp.h"
#include "src/kernel/kernel.h"

extern void _AppCpuResetVector(void);

static volatile uint32_t app_cpu_started = 0;

static void appcpu_unstall(void) {
    REG32(RTC_CNTL_OPTIONS0_REG)     &= ~RTC_CNTL_SW_STALL_APPCPU_C0_M;
    REG32(RTC_CNTL_SW_CPU_STALL_REG) &= ~RTC_CNTL_SW_STALL_APPCPU_C1_M;
}

void start_app_cpu(void) {
    REG32(DPORT_APPCPU_CTRL_D_REG) = (uint32_t)&_AppCpuResetVector;

    REG32(DPORT_APPCPU_CTRL_B_REG) |= DPORT_APPCPU_CLKGATE_EN;

    REG32(DPORT_APPCPU_CTRL_C_REG) &= ~DPORT_APPCPU_RUNSTALL;

    appcpu_unstall();

    REG32(DPORT_APPCPU_CTRL_A_REG) |= DPORT_APPCPU_RESETTING;
    REG32(DPORT_APPCPU_CTRL_A_REG) &= ~DPORT_APPCPU_RESETTING;

    for (uint32_t i = 0; i < 1000000; i++) {
        if (app_cpu_started) {
            uart_print("APP_CPU (core 1) demarre.\n");
            return;
        }
    }

    uart_print("ERREUR : APP_CPU n'a pas demarre (timeout).\n");
}

void app_cpu_main(void) {
    app_cpu_started = 1;

    scheduler_start();
}
