#include "src/interrupts/interrupts.h"
#include "src/scheduler/spinlock.h"

extern char _vector_base[];
static isr_entry_t isr_table[32];
static spinlock_t panic_print_lock = SPINLOCK_INIT;

void interrupts_init_this_core(void) {
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
}

void interrupts_init(void) {
    interrupts_init_this_core();

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

void interrupts_register_handler(uint8_t int_num, isr_handler_t handler, void *arg) {
    if (int_num >= 32) return;
    isr_table[int_num].handler = handler;
    isr_table[int_num].arg = arg;
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
    spinlock_acquire(&panic_print_lock);   /* jamais relâché : un panic est terminal, voir plus bas */

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

    spinlock_release(&panic_print_lock);   /* libère APRES l'affichage : si l'autre coeur panique aussi, il pourra afficher son propre diagnostic au lieu de rester bloqué en silence sur ce verrou */

    while (1) {
        __asm__ volatile ("waiti 0");
    }
}

static int dispatch_pending_interrupts(void) {
    uint32_t pending, enabled;
    __asm__ volatile ("rsr %0, interrupt"  : "=r"(pending));
    __asm__ volatile ("rsr %0, intenable"  : "=r"(enabled));

    uint32_t active = pending & enabled;
    if (active == 0) return 0;

    int handled = 0;
    for (int i = 0; i < 32; i++) {
        if (!(active & (1U << i))) continue;

        if (isr_table[i].handler != 0) {
            isr_table[i].handler(isr_table[i].arg);
            handled = 1;
        }

        __asm__ volatile ("wsr %0, intclear\n rsync\n" :: "r"(1U << i));
    }

    return handled;
}

uint32_t* c_interrupt_handler(uint32_t *sp, uint32_t level) {
    cpu_context_t *ctx = (cpu_context_t *)sp;

    if (level == 1) {
        if (ctx->exccause == EXCCAUSE_SYSCALL) {
            return handle_syscall(ctx);
        }

        if (ctx->exccause != EXCCAUSE_LEVEL1_INTERRUPT) {
            spinlock_acquire(&panic_print_lock);
            uart_print("[diag] EXCCAUSE="); uart_print_hex(ctx->exccause);
            uart_print(" PS="); uart_print_hex(ctx->ps);
            uart_print(" EPC="); uart_print_hex(ctx->epc);
            uart_print("\n");
            spinlock_release(&panic_print_lock);

            int was_user_mode = (ctx->ps & PS_UM_MASK) != 0;
            if (was_user_mode) {
                uart_print("[noyau] tache user terminee (EXCCAUSE=");
                uart_print_hex(ctx->exccause);
                uart_print(", EPC="); uart_print_hex(ctx->epc);
                uart_print(")\n");
                return scheduler_terminate_current(sp);
            }

            panic_dump(ctx);
            return sp;
        }

        if (!dispatch_pending_interrupts()) {
            #define TICK_CYCLES 240000
            set_cpu_private_timer(0, TICK_CYCLES);
            return schedule_next_task(sp);
        }

        return sp;
    }

    if (!dispatch_pending_interrupts()) {
        panic_dump(ctx);
    }

    return sp;
}
