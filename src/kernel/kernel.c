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

void kernel_main(void) {
    mm_init();

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

    uart_print(msg);
    uart_print(alloc);

    while (1) {
        __asm__ volatile ("waiti 0");
    }
}
