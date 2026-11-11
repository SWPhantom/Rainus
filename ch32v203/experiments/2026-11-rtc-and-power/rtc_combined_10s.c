#include "ch32fun.h"
#include "rtc.h"

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

uint32_t clock_seconds(void)
{
    uint32_t high, low;
    // The 32-bit counter is split into 16-bit registers. The low half wraps
    // every 18.2 hours, so retry if the high half changed during the read.
    do {
        high = RTC->CNTH;
        low = RTC->CNTL;
    } while (high != RTC->CNTH);
    return (high << 16) | low;
}

int clock_ticks(uint64_t *ticks)
{
    // The RTC runs while the APB clock is stopped. WCH requires the RTC
    // registers to be re-synchronized before reading them after STOP.
    RTC->CTLRL &= ~RTC_CTLRL_RSF;
    if (!wait_rtc(RTC_CTLRL_RSF, 100)) return 0;
    uint32_t before, after, divider;
    do {
        before = clock_seconds();
        divider = RTC->DIVL;
        after = clock_seconds();
    } while (before != after);
    // The RTC divider counts DOWN from 32767 to zero each second.
    *ticks = ((uint64_t)after << 15) + (32767 - divider);
    return 1;
}

clock_result_t clock_init(uint32_t build_time)
{
    RCC->APB1PCENR |= RCC_APB1Periph_PWR | RCC_APB1Periph_BKP;
    PWR->CTLR |= PWR_CTLR_DBP;
    if ((RCC->BDCTLR & RCC_RTCSEL) &&
        (RCC->BDCTLR & RCC_RTCSEL) != RCC_RTCSEL_LSE)
        return CLOCK_WRONG_SOURCE;
    RCC->BDCTLR |= RCC_LSEON;
    unsigned timeout = 3000;
    while (!(RCC->BDCTLR & RCC_LSERDY)) {
        if (!timeout--) return CLOCK_CRYSTAL_TIMEOUT;
        Delay_Ms(1);
    }
    int newly_enabled = !(RCC->BDCTLR & RCC_RTCEN);
    if (newly_enabled) RCC->BDCTLR |= RCC_RTCSEL_LSE | RCC_RTCEN;
    RTC->CTLRL &= ~RTC_CTLRL_RSF;
    if (!wait_rtc(RTC_CTLRL_RSF, 10000)) return CLOCK_SYNC_TIMEOUT; // Temporary timing experiment.
    // These registers are write-only. Set the known 1 Hz prescaler even
    // when the RTC was already running; the seconds counter is preserved.
    if (!write_begin()) return CLOCK_WRITE_TIMEOUT;
    RTC->PSCRH = 0;
    RTC->PSCRL = 32767;
    if (!write_end()) return CLOCK_WRITE_TIMEOUT;
    // A newly started RTC cannot be assumed to contain a valid timestamp.
    // Otherwise preserve a running clock unless it is earlier than the build.
    if (newly_enabled || clock_seconds() < build_time) {
        if (!write_begin()) return CLOCK_WRITE_TIMEOUT;
        RTC->CNTH = build_time >> 16;
        RTC->CNTL = build_time & 0xffff;
        if (!write_end()) return CLOCK_WRITE_TIMEOUT;
    }
    return CLOCK_OK;
}
