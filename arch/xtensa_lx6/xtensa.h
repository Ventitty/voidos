#ifndef XTENSA_H
#define XTENSA_H

#define UART0_FIFO   (*(volatile uint32_t *)0x3FF40000u)
#define UART0_STATUS (*(volatile uint32_t *)0x3FF4001Cu)

#define TIMERG0_WDTCONFIG0  (*(volatile uint32_t *)0x3FF5F048u)
#define TIMERG0_WDTWPROTECT (*(volatile uint32_t *)0x3FF5F064u)

#define RTC_CNTL_WDTCONFIG0  (*(volatile uint32_t *)0x3FF4808Cu)
#define RTC_CNTL_WDTWPROTECT (*(volatile uint32_t *)0x3FF480A4u)

#endif /* XTENSA_H */

