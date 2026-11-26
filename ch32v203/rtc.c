// RTC = Real-Time Clock: a separate hardware counter driven by the board's
// 32768 Hz crystal, even when the main processor is asleep. We use its counter
// as Unix seconds, not a calendar. This file initializes it and reads whole or
// fractional seconds; it does not format dates or write log records.
//
// Startup: allow clock configuration -> start crystal -> choose it as the
// source -> enable RTC -> synchronize -> set divider -> set time if needed.
// Reads: synchronize with the slow clock, then combine seconds and fraction.
#include "ch32fun.h"
#include "rtc.h"

enum {
    RTC_TICKS_PER_SECOND = 32768,
    RTC_DIVIDER_RELOAD = RTC_TICKS_PER_SECOND - 1,
    RTC_CRYSTAL_START_TIMEOUT_MS = 3000,
    RTC_SETUP_TIMEOUT_MS = 1000,
    RTC_READ_SYNC_TIMEOUT_MS = 100
};

// RTC is a memory-mapped peripheral (hardware at an address), not a normal C
// variable. The -> fields access its hardware registers. CTLRL is its
// low control/status register. The RTC crystal and the CPU peripheral bus run
// independently; a change on the fast CPU side takes time to reach the slow
// RTC side, and vice versa. Status bits tell us when this has finished.
// WCH reference manual sections 6.2.3 and 6.3.2 describe
// the required handshake. Removing these waits can read stale time or lose writes.
//
// Return 1 when hardware sets the requested flag, or 0 after the timeout.
// The limit turns a failed clock/peripheral into a reported fault instead of
// waiting forever. Each check reads the hardware; a delay only happens
// while hardware has not finished. These are failure limits, not sleep timers.
static int wait_for_rtc_flag(uint16_t flag, unsigned timeout_ms)
{
    while (!(RTC->CTLRL & flag)) {
        if (timeout_ms == 0) return 0;
        Delay_Ms(1);
        --timeout_ms;
    }
    return 1;
}

// Enter configuration mode. Return 1 if allowed, 0 if a previous write stalls.
static int begin_clock_configuration(void)
{
    // RTOFF (RTC operation OFF) means the previous write finished. Setting
    // CNF (Configuration) permits changes to the counter/divider registers.
    if (!wait_for_rtc_flag(RTC_CTLRL_RTOFF, RTC_SETUP_TIMEOUT_MS)) return 0;
    RTC->CTLRL |= RTC_CTLRL_CNF;
    return 1;
}

// Commit configuration. Return 1 when the slow-clock hardware has applied it.
static int finish_clock_configuration(void)
{
    // Leaving configuration mode starts the transfer into the RTC domain.
    RTC->CTLRL &= ~RTC_CTLRL_CNF;
    return wait_for_rtc_flag(RTC_CTLRL_RTOFF, RTC_SETUP_TIMEOUT_MS);
}

uint32_t clock_seconds(void)
{
    uint32_t high, low;
    // The 32-bit counter is split into 16-bit registers. The low half wraps
    // every 18.2 hours, so retry if the high half changed during the read.
    // CNTH/CNTL mean Counter High/Low. Shifting high by 16 bits places it
    // above the low 16 bits; OR joins the halves into one 32-bit timestamp.
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
    // RSF (Registers Synchronized Flag) means the CPU-visible register values
    // have caught up with the independently running RTC counter.
    // Clear it and wait for a fresh synchronization. This is needed after
    // STOP; the current implementation also does it on edges while awake.
    // That keeps one read path, but each unsuccessful poll costs 1 ms in the
    // interrupt. A sync failure can hold this handler for up to 100 ms.
    RTC->CTLRL &= ~RTC_CTLRL_RSF;
    if (!wait_for_rtc_flag(RTC_CTLRL_RSF, RTC_READ_SYNC_TIMEOUT_MS)) return 0;
    uint32_t before, after, divider;
    // Retry if the second changed while reading the fractional part, so we
    // do not combine one second's counter with another second's divider.
    do {
        before = clock_seconds();
        divider = RTC->DIVL;
        after = clock_seconds();
    } while (before != after);
    // The RTC divider counts DOWN from 32767 to zero each second.
    *ticks = (uint64_t)after * RTC_TICKS_PER_SECOND + (RTC_DIVIDER_RELOAD - divider);
    return 1;
}

clock_result_t clock_init(uint32_t build_time)
{
    // RCC (Reset and Clock Control) enables access to the PWR (Power Control)
    // and BKP (Backup) peripherals. APB1PCENR is the peripheral-clock-enable
    // register. DBP removes write protection on the RTC/backup configuration.
    // This does not read/write a backup scratchpad or provide backup power.
    RCC->APB1PCENR |= RCC_APB1Periph_PWR | RCC_APB1Periph_BKP;
    PWR->CTLR |= PWR_CTLR_DBP;
    // BDCTLR is the backup-domain control register. RTCSEL holds the selected
    // clock source. LSE (Low-Speed External) is our 32768 Hz crystal; refuse
    // to reinterpret an already configured counter from a different source.
    if ((RCC->BDCTLR & RCC_RTCSEL) &&
        (RCC->BDCTLR & RCC_RTCSEL) != RCC_RTCSEL_LSE)
        return CLOCK_WRONG_SOURCE;
    RCC->BDCTLR |= RCC_LSEON; // Set LSE ON; LSERDY below reports crystal ready.
    // The external crystal takes time to start after power-on. A missing or
    // failed crystal must report an error rather than stall startup forever.
    unsigned timeout = RTC_CRYSTAL_START_TIMEOUT_MS;
    while (!(RCC->BDCTLR & RCC_LSERDY)) {
        if (timeout == 0) return CLOCK_CRYSTAL_TIMEOUT;
        Delay_Ms(1);
        --timeout;
    }
    int newly_enabled = !(RCC->BDCTLR & RCC_RTCEN); // RTCEN is the RTC enable bit.
    if (newly_enabled) {
        // Select a clock source before enabling the RTC clock. The chip can
        // ignore RTCEN when no source has been selected yet.
        RCC->BDCTLR |= RCC_RTCSEL_LSE;
        RCC->BDCTLR |= RCC_RTCEN;
    }
    RTC->CTLRL &= ~RTC_CTLRL_RSF;
    if (!wait_for_rtc_flag(RTC_CTLRL_RSF, RTC_SETUP_TIMEOUT_MS)) return CLOCK_SYNC_TIMEOUT;
    // These registers are write-only. Set the known 1 Hz prescaler even
    // when the RTC was already running; the seconds counter is preserved.
    // PSCRH/PSCRL are the prescaler (clock divider) High/Low registers:
    // 32768 crystal pulses produce one increment of the seconds counter.
    if (!begin_clock_configuration()) return CLOCK_WRITE_TIMEOUT;
    RTC->PSCRH = 0;
    RTC->PSCRL = RTC_DIVIDER_RELOAD;
    if (!finish_clock_configuration()) return CLOCK_WRITE_TIMEOUT;
    // A newly started RTC cannot be assumed to contain a valid timestamp.
    // Otherwise preserve a running clock unless it is earlier than the build.
    if (newly_enabled || clock_seconds() < build_time) {
        if (!begin_clock_configuration()) return CLOCK_WRITE_TIMEOUT;
        RTC->CNTH = build_time >> 16;
        RTC->CNTL = build_time & 0xffff;
        if (!finish_clock_configuration()) return CLOCK_WRITE_TIMEOUT;
    }
    return CLOCK_OK;
}
