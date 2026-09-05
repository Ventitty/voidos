#ifndef XTENSA_H
#define XTENSA_H

#define UART0_FIFO   (*(volatile uint32_t *)0x3FF40000u)
#define UART0_STATUS (*(volatile uint32_t *)0x3FF4001Cu)
#define UART0_STATUS_REG      (*(volatile uint32_t *)(0x3FF40000u + 0x001C))
#define UART0_RX_FIFO_CNT     ((*(volatile uint32_t *)(0x3FF40000u + 0x001C)) & 0xFFu)

#define RTC_CNTL_WDTCONFIG0_REG   0x3FF4808Cu   /* était (par erreur) 0x...090 */
#define RTC_CNTL_WDTCONFIG1_REG   0x3FF48090u   /* était (par erreur) 0x...094 */
#define RTC_CNTL_WDTFEED_REG      0x3FF480A0u   /* était (par erreur) 0x...0A4 */
#define RTC_CNTL_WDTWPROTECT_REG  0x3FF480A4u   /* était (par erreur) 0x...0A8 */
#define RTC_CNTL_WDT_WKEY         0x50D83AA1u

/* ---- TIMG0 / TIMG1 (Main System Watchdogs) -------------------------------
 * Ces offsets-là étaient déjà corrects dans la version précédente. */
#define TIMG0_WDTCONFIG0_REG      0x3FF5F048u
#define TIMG0_WDTFEED_REG         0x3FF5F060u
#define TIMG0_WDTWPROTECT_REG     0x3FF5F064u

#define TIMG1_WDTCONFIG0_REG      0x3FF60048u
#define TIMG1_WDTFEED_REG         0x3FF60060u
#define TIMG1_WDTWPROTECT_REG     0x3FF60064u

#define WDT_FEED_MAGIC            0xABAD1DEAu

#endif /* XTENSA_H */

