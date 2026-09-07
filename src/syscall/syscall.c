#include "src/syscall/syscall.h"
#include "src/interrupts/interrupts.h"
#include "src/scheduler/scheduler.h"
#include "src/kernel/kernel.h"

uint32_t *handle_syscall(cpu_context_t *ctx) {
    ctx->epc += 3;

    uint32_t num = ctx->a2;
    uint32_t arg0 = ctx->a3;

    switch (num) {
        case SYS_YIELD:
            ctx->a2 = 0;
            return schedule_next_task((uint32_t *)ctx);

        case SYS_PRINT:
            uart_print((const char *)arg0);
            ctx->a2 = 0;
            return (uint32_t *)ctx;

        case SYS_EXIT:
            return scheduler_terminate_current((uint32_t *)ctx);

        default:
            ctx->a2 = (uint32_t)-1;
            return (uint32_t *)ctx;
    }
}
