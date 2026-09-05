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
    /* Purge tout ce qui traîne dans le RX FIFO au démarrage (résidus de la
     * session de flash esptool sur la même UART) et acquitte les éventuelles
     * erreurs déjà levées (overflow / framing / break). Sans ça, un octet
     * corrompu ou un flag d'erreur déjà actif peut faire croire en
     * permanence à UART0_RX_FIFO_CNT qu'un octet est disponible -> le
     * prompt se réaffiche en boucle infinie sans qu'aucune vraie touche
     * n'ait été pressée. */
    UART0_CONF0_REG |= UART_RXFIFO_RST;
    UART0_CONF0_REG &= ~UART_RXFIFO_RST;
    UART0_INT_CLR_REG = UART_RX_ERROR_MASK;

    uart_print("Input > ");

    while (1) {
        /* Si une erreur RX est levée (overflow/framing/break), le FIFO peut
         * rester dans un état incohérent : on l'acquitte et on repurge
         * plutôt que de risquer de boucler sur un octet fantôme. */
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

    echo();

    while (1) {
        __asm__ volatile ("waiti 0");
    }
}
