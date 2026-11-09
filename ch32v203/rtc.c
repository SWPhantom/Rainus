#include "ch32fun.h"
#include "rtc.h"

volatile unsigned rtc_debug_step;

static int wait_rtc(uint16_t bit, unsigned timeout_ms)
{
    while (!(RTC->CTLRL & bit)) {
        if (!timeout_ms--) return 0;
        Delay_Ms(1);
    }
    return 1;
}

static int write_begin(void)
{
    if (!wait_rtc(RTC_CTLRL_RTOFF, 1000)) return 0;
    RTC->CTLRL |= RTC_CTLRL_CNF;
    return 1;
}

static int write_end(void)
{
    RTC->CTLRL &= ~RTC_CTLRL_CNF;
    return wait_rtc(RTC_CTLRL_RTOFF, 1000);
}

int clock_init(void)
{
    rtc_debug_step = 1; // Validate the previous RTC clock selection.
    RCC->APB1PCENR |= RCC_APB1Periph_PWR | RCC_APB1Periph_BKP;
    PWR->CTLR |= PWR_CTLR_DBP;
    // Never reset the backup domain as part of normal initialization.
    if ((RCC->BDCTLR & RCC_RTCSEL) &&
        (RCC->BDCTLR & RCC_RTCSEL) != RCC_RTCSEL_LSE) return 0;
    RCC->BDCTLR |= RCC_LSEON;
    rtc_debug_step = 2; // Wait for the 32.768 kHz crystal.
    unsigned timeout = 3000;
    while (!(RCC->BDCTLR & RCC_LSERDY)) {
        if (!timeout--) return 0;
        Delay_Ms(1);
    }
    if (!(RCC->BDCTLR & RCC_RTCEN)) {
        rtc_debug_step = 3; // Start and synchronize the RTC.
        RCC->BDCTLR |= RCC_RTCSEL_LSE | RCC_RTCEN;
        RTC->CTLRL &= ~RTC_CTLRL_RSF;
        if (!wait_rtc(RTC_CTLRL_RSF, 1000)) return 0;
        rtc_debug_step = 4; // Enter RTC configuration mode.
        if (!write_begin()) return 0;
        RTC->PSCRH = 0;
        RTC->PSCRL = 32767;
        rtc_debug_step = 5; // Wait for the prescaler write to finish.
        if (!write_end()) return 0;
        BKP->DATAR1 = 0; // RTC is running but has no known UTC time.
    } else {
        rtc_debug_step = 6; // Synchronize an already-running RTC.
        RTC->CTLRL &= ~RTC_CTLRL_RSF;
        if (!wait_rtc(RTC_CTLRL_RSF, 1000)) return 0;
    }
    rtc_debug_step = 99;
    return 1;
}

time_source_t clock_source(void) { return time_source_from_marker(BKP->DATAR1); }

uint32_t clock_seconds(void)
{
    uint32_t high, low;
    do {
        high = RTC->CNTH;
        low = RTC->CNTL;
    } while (high != RTC->CNTH);
    return (high << 16) | low;
}

int clock_set(uint32_t seconds, time_source_t source)
{
    if (source != TIME_BUILD && source != TIME_SET) return 0;
    BKP->DATAR1 = 0;
    if (!write_begin()) return 0;
    RTC->CNTH = seconds >> 16;
    RTC->CNTL = seconds & 0xffff;
    if (!write_end()) return 0;
    BKP->DATAR1 = source == TIME_BUILD ? TIME_MARKER_BUILD : TIME_MARKER_SET;
    return 1;
}
