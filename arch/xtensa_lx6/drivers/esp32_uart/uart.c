#include "arch/xtensa_lx6/drivers/esp32_uart/uart.h"

void uart0_putchar(char c) {
    while (((UART0_STATUS >> 16) & 0xFFu) >= 128) { }
    UART0_FIFO = (uint32_t)c;
}

int uart0_write(const void *buf, uint32_t len) {
    const uint8_t *in = (const uint8_t *)buf;
    for (uint32_t i = 0; i < len; i++) {
        uart0_putchar((char)in[i]);
    }
    return (int)len;
}

int uart0_read(void *buf, uint32_t len) {
    uint8_t *out = (uint8_t *)buf;
    uint32_t n = 0;
    while (n < len && UART0_RX_FIFO_CNT > 0) {
        out[n++] = (uint8_t)(UART0_FIFO & 0xFFu);
    }
    return (int)n;
}
