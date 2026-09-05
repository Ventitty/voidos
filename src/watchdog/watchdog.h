#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "src/types.h"
#include "arch/xtensa_lx6/xtensa.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ---- RTC_CNTL (RTC Watchdog + Super Watchdog) ---------------------------- */

void wdt_disable_all(void);

#endif /* WATCHDOG_H */
