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

void kernel_main(void) {
    wdt_disable_all();
    mm_init();
    interrupts_init();
    scheduler_init();
    start_app_cpu();

    task_create(echo);

    scheduler_start();
}
