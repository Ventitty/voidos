#ifndef KERNEL_H
#define KERNEL_H

#include "src/types.h"
#include "src/memory_manager/memory.h"
#include "arch/xtensa_lx6/xtensa.h"

void uart_print(const char *str);
void uart_print_hex(uint32_t val);

#endif /* KERNEL_H */

