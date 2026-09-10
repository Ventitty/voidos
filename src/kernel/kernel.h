#ifndef KERNEL_H
#define KERNEL_H

#include "src/types.h"
#include "src/memory_manager/memory.h"
#include "arch/xtensa_lx6/xtensa.h"
#include "src/utils/utils.h"
#include "src/interrupts/interrupts.h"
#include "src/watchdog/watchdog.h"
#include "src/smp/smp.h"
#include "src/scheduler/scheduler.h"
#include "src/scheduler/mutex.h"
#include "src/scheduler/semaphore.h"
#include "src/scheduler/cond.h"
#include "src/scheduler/event_group.h"
#include "src/syscall/syscall.h"
#include "src/drivers/esp32_led/led.h"
#include "src/drivers/esp32_gpio/gpio.h"
#include "src/drivers/esp32_spi/spi.h"
#include "src/drivers/esp32_sdcard/sdcard.h"

void uart_print(const char *str);
void uart_print_hex(uint32_t val);

#endif /* KERNEL_H */
