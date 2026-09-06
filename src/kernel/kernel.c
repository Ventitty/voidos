#include "src/kernel/kernel.h"

void uart_putchar(char c) {
    while (((UART0_STATUS >> 16) & 0xFF) >= 128);

    UART0_FIFO = (uint32_t)c;
}

void uart_print(const char *str) {
    while (*str != '\0') {
        if (*str == '\n') {
            uart_putchar('\r');
        }
        uart_putchar(*str);
        str++;
    }
}

void uart_print_hex(uint32_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    uart_print("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putchar(hex_chars[(val >> i) & 0xF]);
    }
}

void echo(void) {
    UART0_CONF0_REG |= UART_RXFIFO_RST;
    UART0_CONF0_REG &= ~UART_RXFIFO_RST;
    UART0_INT_CLR_REG = UART_RX_ERROR_MASK;

    uart_print("Input > ");

    while (1) {
        uint32_t int_st = UART0_INT_ST_REG;
        if (int_st & UART_RX_ERROR_MASK) {
            UART0_INT_CLR_REG = UART_RX_ERROR_MASK;
            UART0_CONF0_REG |= UART_RXFIFO_RST;
            UART0_CONF0_REG &= ~UART_RXFIFO_RST;
            continue;
        }

        if (UART0_RX_FIFO_CNT > 0) {
            char c = (char)(UART0_FIFO & 0xFFu);

            if (c == '\r' || c == '\n') {
                uart_putchar('\r');
                uart_putchar('\n');
                uart_print("Input > ");
            } else if (c == '\b' || c == 0x7F) {
                uart_print("\b \b");
            } else {
                uart_putchar(c);
            }
        }
    }
}


void task_a(void) {
    while (1) {
        uint32_t core = get_core_id();
        uart_print("[Tache A - core ");
        if (core == 0) {
            uart_print("0");
        } else {
            uart_print("1");
        }
        uart_print("]\n");

        for (volatile int i = 0; i < 500000; i++) { __asm__ volatile ("nop"); }
    }
}

void task_b(void) {
    while (1) {
        uint32_t core = get_core_id();
        uart_print("[Tache B - core ");
        if (core == 0) {
            uart_print("0");
        } else {
            uart_print("1");
        }
        uart_print("]\n");

        for (volatile int i = 0; i < 500000; i++) { __asm__ volatile ("nop"); }
    }
}

void kernel_main(void) {
    wdt_disable_all();
    mm_init();
    interrupts_init();
    scheduler_init();
    start_app_cpu();

    const char *msg = "Hello world !\n";
    size_t len = 0;
    while (msg[len] != '\0') len++;

    char *alloc = nmap(len + 1);
    if (alloc == NULL) {
        uart_print("ERREUR : Allocation memoire (nmap) a echoue !\n");
        while(1);
    }

    size_t i = 0;
    for (; i < len; i++) alloc[i] = msg[i];
    alloc[i] = '\0';

    uart_print(alloc);

    task_create(task_a);
    task_create(task_b);
    task_create(echo);

    scheduler_start();
}
