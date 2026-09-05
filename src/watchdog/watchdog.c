#include "src/watchdog/watchdog.h"

static void rtc_wdt_disable(void) {
    REG32(RTC_CNTL_WDTWPROTECT_REG) = RTC_CNTL_WDT_WKEY;   /* déverrouille l'accès */
    REG32(RTC_CNTL_WDTFEED_REG)     = WDT_FEED_MAGIC;       /* nourrit une dernière fois */
    REG32(RTC_CNTL_WDTCONFIG0_REG)  = 0;                    /* WDT_EN = 0 -> désactivé */
    REG32(RTC_CNTL_WDTCONFIG1_REG)  = 0;
    REG32(RTC_CNTL_WDTWPROTECT_REG) = 0;                    /* reverrouille */
}

static void timg_wdt_disable(uint32_t wdtconfig0, uint32_t wdtfeed, uint32_t wdtwprotect) {
    REG32(wdtwprotect) = RTC_CNTL_WDT_WKEY;   /* même clé pour TIMG0/1 */
    REG32(wdtfeed)     = WDT_FEED_MAGIC;
    REG32(wdtconfig0)  = 0;
    REG32(wdtwprotect) = 0;
}

void wdt_disable_all(void) {
    rtc_wdt_disable();
    timg_wdt_disable(TIMG0_WDTCONFIG0_REG, TIMG0_WDTFEED_REG, TIMG0_WDTWPROTECT_REG);
    timg_wdt_disable(TIMG1_WDTCONFIG0_REG, TIMG1_WDTFEED_REG, TIMG1_WDTWPROTECT_REG);
}
