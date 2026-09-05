#include "src/interrupts/interrupts.h"

extern char _vector_base[];
static isr_entry_t isr_table[32];

void interrupts_init(void) {
    uint32_t vecbase = (uint32_t)_vector_base;
    __asm__ volatile (
        "wsr %0, vecbase\n"
        "rsync\n"
        :: "r"(vecbase)
    );

    __asm__ volatile (
        "wsr %0, intenable\n"
        "rsync\n"
        :: "r"(0)
    );

    __asm__ volatile (
        "wsr %0, intclear\n"
        "rsync\n"
        :: "r"(0xFFFFFFFF)
    );

    for (int i = 0; i < 32; i++) {
        isr_table[i].handler = 0;
        isr_table[i].arg = 0;
    }
}

void interrupts_enable_line(uint8_t int_num) {
    if (int_num >= 32) return;

    uint32_t intenable;
    __asm__ volatile ("rsr %0, intenable" : "=r"(intenable));
    intenable |= (1U << int_num);
    __asm__ volatile (
        "wsr %0, intenable\n"
        "rsync\n"
        :: "r"(intenable)
    );
}

void interrupts_enable_global(void) {
    __asm__ volatile (
        "rsil a2, 0\n"
        ::: "a2"
    );
}

void interrupts_disable_global(void) {
    __asm__ volatile (
        "rsil a2, 15\n"
        ::: "a2"
    );
}

static void panic_dump(const cpu_context_t *ctx) {
    uart_print("\n================ PANIC KERNEL ================\n");
    uart_print(" EXCEPTION FATALE : Cause = "); uart_print_hex(ctx->exccause); uart_print("\n");
    uart_print(" EPC  (PC fautif) : "); uart_print_hex(ctx->epc); uart_print("\n");
    uart_print(" PS  (Processor)  : "); uart_print_hex(ctx->ps); uart_print("\n");
    uart_print("----------------------------------------------\n");
    uart_print(" Registres :\n");
    uart_print("   a0 (Return Addr) : "); uart_print_hex(ctx->a0); uart_print("\n");
    uart_print("   a1 (Stack Orig)  : "); uart_print_hex(ctx->sp_orig); uart_print("\n");
    uart_print("   a2 : "); uart_print_hex(ctx->a2); uart_print(" | a3 : "); uart_print_hex(ctx->a3); uart_print("\n");
    uart_print("==============================================\n");

    while (1) {
        __asm__ volatile ("waiti 0");
    }
}

uint32_t* c_interrupt_handler(uint32_t *sp) {
    cpu_context_t *ctx = (cpu_context_t *)sp;

    if (ctx->exccause == EXCCAUSE_LEVEL1_INTERRUPT) {
        #define TICK_CYCLES 240000
        set_cpu_private_timer(0, TICK_CYCLES);

        return sp;
    }

    panic_dump(ctx);

    return sp;
}
