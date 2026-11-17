#include "ch32fun.h"
#include "rtc.h"

// RTC is a memory-mapped peripheral, not a normal C variable. CTLRL is its
// low control/status register. The RTC crystal and the CPU peripheral bus run
// independently; status bits tell us when a read or write has crossed between
// those clock domains. WCH reference manual sections 6.2.3 and 6.3.2 describe
// the required handshake. Removing these waits can read stale time or lose writes.
//
// Return 1 when hardware sets the requested flag, or 0 after the timeout.
// The limit turns a failed clock/peripheral into a reported fault instead of
// waiting forever. These limits are failure budgets, not routine sleep periods.
static int wait_for_rtc_flag(uint16_t flag, unsigned timeout_ms)
{
    while (!(RTC->CTLRL & flag)) {
        if (timeout_ms == 0) return 0;
        Delay_Ms(1);
        --timeout_ms;
    }
    return 1;
}

static int write_begin(void)
{
    // RTOFF=1 means the previous write finished. CNF=1 permits configuration.
    if (!wait_for_rtc_flag(RTC_CTLRL_RTOFF, 1000)) return 0;
    RTC->CTLRL |= RTC_CTLRL_CNF;
    return 1;
}

static int write_end(void)
{
    // Leaving configuration mode starts the transfer into the RTC domain.
    RTC->CTLRL &= ~RTC_CTLRL_CNF;
    return wait_for_rtc_flag(RTC_CTLRL_RTOFF, 1000);
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
    // Seconds alone are too coarse for the 100 ms bounce filter. Combine
    // whole Unix seconds with the RTC divider for subsecond resolution.
    // This does NOT change the RTC rate or the one-second CSV timestamps.
    //
    // RSF=1 means the bus-side registers have synchronized with the RTC.
    // Clear it and wait for a fresh synchronization. This is needed after
    // STOP; the current implementation also does it on edges while awake.
    // That keeps one read path, but each unsuccessful poll costs 1 ms in the
    // interrupt. A sync failure can hold this handler for up to 100 ms.
    RTC->CTLRL &= ~RTC_CTLRL_RSF;
    if (!wait_for_rtc_flag(RTC_CTLRL_RSF, 100)) return 0;
    uint32_t before, after, divider;
    // Retry if the second changed while reading the fractional part, so we
    // do not combine one second's counter with another second's divider.
    do {
        before = clock_seconds();
        divider = RTC->DIVL;
        after = clock_seconds();
    } while (before != after);
    // The RTC divider counts DOWN from 32767 to zero each second.
    *ticks = (uint64_t)after * 32768 + (32767 - divider);
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
    // The external crystal takes time to start after power-on. A missing or
    // failed crystal must report an error rather than stall startup forever.
    unsigned timeout = 3000;
    while (!(RCC->BDCTLR & RCC_LSERDY)) {
        if (timeout == 0) return CLOCK_CRYSTAL_TIMEOUT;
        Delay_Ms(1);
        --timeout;
    }
    int newly_enabled = !(RCC->BDCTLR & RCC_RTCEN);
    if (newly_enabled) {
        // Select a clock source before enabling the RTC clock. The chip can
        // ignore RTCEN when no source has been selected yet.
        RCC->BDCTLR |= RCC_RTCSEL_LSE;
        RCC->BDCTLR |= RCC_RTCEN;
    }
    RTC->CTLRL &= ~RTC_CTLRL_RSF;
    if (!wait_for_rtc_flag(RTC_CTLRL_RSF, 1000)) return CLOCK_SYNC_TIMEOUT;
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
