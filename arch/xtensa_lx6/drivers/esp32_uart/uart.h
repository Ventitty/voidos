#ifndef UART_H
#define UART_H

#include "src/types.h"
#include "arch/xtensa_lx6/includes/xtensa.h"

void uart0_putchar(char c);
int  uart0_write(const void *buf, uint32_t len);
int  uart0_read(void *buf, uint32_t len);

#endif /* UART_H */
