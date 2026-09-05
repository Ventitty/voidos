#include "src/utils/utils.h"
#include "src/kernel/kernel.h"
#include "src/interrupts/interrupts.h"

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

void kernel_main(void) {
    mm_init();
    interrupts_init();

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

    __asm__ volatile ("ill");

    volatile int a = 42;
    volatile int b = 0;
    volatile int c = a / b;

    (void)c;

    while (1) {
        __asm__ volatile ("waiti 0");
    }
}
