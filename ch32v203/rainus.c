#include "ch32fun.h"
#include "fatfs/ff.h"
#include "rtc.h"
#include "event.h"
#include "chip_id.h"
#include "log.h"
#include <stdio.h>
#if RAINUS_DEBUG_USB
#include "usbd.h"
#endif

// Set to 0 to disable all firmware-controlled LED flashes for field use.
#define RAINUS_LED_ENABLED 1

#ifndef RAINUS_BUILD_UNIX_TIME
#error Build with the Makefile to provide a UTC build time.
#endif
#if RAINUS_BUILD_UNIX_TIME < 1577836800UL || RAINUS_BUILD_UNIX_TIME > 4294967295UL
#error Build time must be a valid Unix timestamp between 2020 and 2106.
#endif
#ifndef LOG_BUFFER_LENGTH
#define LOG_BUFFER_LENGTH 10
#endif
#if LOG_BUFFER_LENGTH < 1
#error LOG_BUFFER_LENGTH must be a positive integer.
#endif
#define INPUT_LINES (EXTI_Line0 | EXTI_Line2)

// Bits in the interrupt controller's system-control register.
#define PFIC_SLEEP_DEEP  (1u << 2)
#define PFIC_WFI_AS_WFE  (1u << 3)
#define PFIC_SET_EVENT   (1u << 5)

// These numbers are also the repeating LED error codes and USB error values.
typedef enum {
    ERROR_RTC = 2,
    ERROR_SD_MOUNT = 3,
    ERROR_QUEUE_FULL = 4,
    ERROR_SD_WRITE = 5
} logger_error_t;

static FATFS filesystem;
static char chip_id[CHIP_ID_TEXT_LEN + 1];
// Tips and flush markers share one queue, preserving their processing order.
// Both input interrupts have the same priority and cannot preempt each other.
// Foreground code masks interrupts while examining/removing queued items.
static event_queue_t event_queue;
static volatile clock_result_t clock_fault;
static debounce_t tip_debounce, flush_debounce;
// The batch serves a different purpose from the queue: group timestamps into
// one SD operation to save energy. Its contents stay fixed during that write,
// while the interrupt can still add newly arriving events to event_queue.
// All this RAM is lost on reset or power loss. Static variables start at zero.
static uint32_t batch[LOG_BUFFER_LENGTH];
static unsigned batch_count;
#if RAINUS_DEBUG_USB
static volatile int status_requested;
static unsigned saved_count;
static unsigned fatal_error;
static unsigned clock_error;
static volatile unsigned interrupt_count;
void HandleUSBInput(int count, uint8_t *data)
{
    for (int i = 0; i < count; ++i)
        if (data[i] == '?' || data[i] == 's' || data[i] == 'i') status_requested = 1;
}
static void debug_status(void)
{
    if (!status_requested || !USBDCTX.USBD_DevEnumStatus) return;
    char line[150];
    int n = snprintf(line, sizeof line, "id=%s error=%u rtc=%u irq=%u pin=%d saved=%u buffered=%u time=%lu\r\n",
                     chip_id, fatal_error, clock_error, interrupt_count, funDigitalRead(PA0),
                     saved_count, batch_count, (unsigned long)clock_seconds());
    if (n > 0 && (unsigned)n < sizeof line &&
        USBD_SendEndpointNEW(3, (uint8_t *)line, n, 1) == 0) status_requested = 0;
}
#endif

// Optional USB work runs synchronously in the foreground. This is a no-op
// in the field build; the debug build deliberately stays awake for USB.
static void service_debug(void)
{
#if RAINUS_DEBUG_USB
    poll_input();
    debug_status();
#endif
}

#if RAINUS_LED_ENABLED
static void led(int on) { funDigitalWrite(PA15, on ? FUN_LOW : FUN_HIGH); }
#endif

// Only an explicit manual flush gets a success indication. Interrupts stay
// enabled during these short delays, so new tips continue entering the queue.
static void acknowledge_flush(void)
{
#if RAINUS_LED_ENABLED
    led(1); Delay_Ms(75); led(0); Delay_Ms(75);
    led(1); Delay_Ms(75); led(0);
#endif
}

// Terminal error policy: stop accepting switch events and repeat the error
// code until reset. LED delays block here; USB still runs between groups.
// We do not write a sensor record with an untrusted RTC or retry a partial
// SD batch (which could duplicate rows that were already saved).
static void stop_with_error(logger_error_t error)
{
    NVIC_DisableIRQ(EXTI0_IRQn);
    NVIC_DisableIRQ(EXTI2_IRQn);
#if RAINUS_DEBUG_USB
    fatal_error = error;
#endif
    (void)error;
    for (;;) {
#if RAINUS_LED_ENABLED
        for (unsigned i = 0; i < (unsigned)error; ++i) {
            led(1); Delay_Ms(150); led(0); Delay_Ms(200);
        }
#endif
        service_debug();
        Delay_Ms(1200);
    }
}

// Shared capture path. Each pin has its own debounce history, so the service
// button cannot debounce away a nearby rain tip (or vice versa). No SD I/O
// happens here. Same-priority handlers serialize access to the RTC and queue.
static void capture_edge(event_kind_t kind, debounce_t *debounce)
{
    uint64_t ticks;
    if (!clock_ticks(&ticks)) {
        clock_fault = CLOCK_SYNC_TIMEOUT;
        return;
    }
    debounce_result_t result = debounce_edge(debounce, ticks);
    if (result == DEBOUNCE_CLOCK_BACKWARDS) {
        clock_fault = CLOCK_MOVED_BACKWARDS;
        return;
    }
    if (result == DEBOUNCE_IGNORED) return;
    // Divide 32768-Hz ticks by 32768 (2^15) for whole Unix seconds.
    input_event_t event = {kind, (uint32_t)(ticks >> 15)};
    event_queue_push(&event_queue, event); // A full queue latches the fault.
}

// These handlers also run during startup SD preparation and batch writes.
__attribute__((interrupt)) void EXTI0_IRQHandler(void)
{
    if (!(EXTI->INTFR & EXTI_Line0)) return;
    EXTI->INTFR = EXTI_Line0;
#if RAINUS_DEBUG_USB
    ++interrupt_count;
#endif
    capture_edge(EVENT_TIP, &tip_debounce);
}

__attribute__((interrupt)) void EXTI2_IRQHandler(void)
{
    if (!(EXTI->INTFR & EXTI_Line2)) return;
    EXTI->INTFR = EXTI_Line2;
    capture_edge(EVENT_FLUSH, &flush_debounce);
}

static void enable_switch_interrupts(void)
{
    // Dry contacts from PA0/PA2 to GND pull the normally-high inputs LOW.
    funPinMode(PA0, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA0, FUN_HIGH);
    funPinMode(PA2, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA2, FUN_HIGH);
    AFIO->EXTICR[0] &= ~(AFIO_EXTICR1_EXTI0 | AFIO_EXTICR1_EXTI2);
    EXTI->RTENR &= ~INPUT_LINES;
    EXTI->FTENR |= INPUT_LINES;
    EXTI->INTFR = INPUT_LINES;
    EXTI->INTENR |= INPUT_LINES;
    // Also latch an event for WFE, even while interrupts are masked.
    EXTI->EVENR |= INPUT_LINES;
    NVIC_SetPriority(EXTI0_IRQn, 0);
    NVIC_SetPriority(EXTI2_IRQn, 0);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI2_IRQn);
}

// A compiler barrier emits no CPU instruction. It keeps memory accesses on
// their intended side of interrupt masking; ch32fun's irq helpers do not
// provide that guarantee themselves. This is unrelated to waiting or sleep.
static inline void compiler_barrier(void)
{
    __asm__ volatile ("" ::: "memory");
}

#if !RAINUS_DEBUG_USB
// Called only with interrupts masked and an empty software queue.
// WFE = Wait For Event. EXTI latches either input as a hardware event even with
// interrupts masked, so an edge between the empty check and sleep can wake us.
// This QingKe core uses the RISC-V `wfi` instruction for BOTH WFI and WFE;
// PFIC_WFI_AS_WFE chooses event behavior. PFIC_SLEEP_DEEP selects deep sleep,
// and PWR->CTLR selects STOP rather than standby. STOP retains our RAM.
static void sleep_until_switch_event(void)
{
    PWR->CTLR &= ~(PWR_CTLR_PDDS | PWR_CTLR_LPDS);

    // Set and consume a software event first. An old, uncleared event would
    // otherwise make sleep return immediately and waste power. This sequence
    // is the one that reduced idle current in our hardware comparisons.
    NVIC->SCTLR |= PFIC_SLEEP_DEEP | PFIC_WFI_AS_WFE | PFIC_SET_EVENT;
    NVIC->SCTLR &= ~PFIC_SET_EVENT;
    __asm__ volatile ("wfi" ::: "memory");

    // If a switch edge arrived during that first wait, let the interrupt run
    // now. Otherwise actually sleep; any edge after this check wakes us.
    // Keep this check instead of ch32fun's __WFE(), which always waits twice.
    if (!(EXTI->INTFR & INPUT_LINES))
        __asm__ volatile ("wfi" ::: "memory");
    NVIC->SCTLR &= ~(PFIC_SLEEP_DEEP | PFIC_WFI_AS_WFE);
}
#endif

// Block until one accepted tip or flush request is available, or stop on a fault.
// In the field build an empty queue leads to STOP, not periodic pin polling.
// A wake can occur without a queued timestamp (e.g. a filtered bounce), so
// check again after waking. Return with interrupts enabled before any SD work.
static input_event_t wait_for_event(void)
{
    for (;;) {
        service_debug();
        __disable_irq();
        compiler_barrier();

        if (clock_fault || event_queue.overflow) {
            logger_error_t error = clock_fault ? ERROR_RTC : ERROR_QUEUE_FULL;
#if RAINUS_DEBUG_USB
            if (clock_fault) clock_error = clock_fault;
#endif
            compiler_barrier();
            __enable_irq();
            stop_with_error(error);
        }
        input_event_t event;
        if (event_queue_pop(&event_queue, &event)) {
            compiler_barrier();
            __enable_irq();
            return event;
        }

#if !RAINUS_DEBUG_USB
        sleep_until_switch_event();
#endif
        compiler_barrier();
        __enable_irq(); // Now run any interrupt that woke us and queued an event.
    }
}

int main(void)
{
    SystemInit();
    funGpioInitAll();
    // The factory ID occupies 12 consecutive bytes, not a generated ID.
    const volatile uint8_t *uid = (const volatile uint8_t *)&ESIG->UID0;
    uint8_t bytes[CHIP_ID_BYTES];
    for (unsigned i = 0; i < CHIP_ID_BYTES; ++i) bytes[i] = uid[i];
    chip_id_format(bytes, chip_id);
    funDigitalWrite(PA15, FUN_HIGH);
    funPinMode(PA15, GPIO_CFGLR_OUT_10Mhz_PP);
#if RAINUS_DEBUG_USB
    USBDSetup();
#endif
    // The RTC must initialize successfully before switch capture is enabled.
    clock_result_t result = clock_init(RAINUS_BUILD_UNIX_TIME);
    if (result != CLOCK_OK) {
#if RAINUS_DEBUG_USB
        clock_error = result;
#endif
        stop_with_error(ERROR_RTC);
    }
    // Capture starts after the RTC is ready. Inputs can queue events while SD
    // startup is busy; the main loop processes them after log_open() finishes.
    enable_switch_interrupts();
    if (f_mount(&filesystem, "", 1) != FR_OK) stop_with_error(ERROR_SD_MOUNT);
    // Pay the initial directory lookup / end-of-file traversal here, rather
    // than on the first data batch. Opening an empty file writes no CSV rows.
    if (!log_open()) stop_with_error(ERROR_SD_WRITE);

    // Initialization above runs once per reset. This loop runs for the whole
    // deployment: wait/asleep -> collect a tip or service a flush request.
    // SD writes block this loop, but both input interrupts stay enabled.
    for (;;) {
        input_event_t event = wait_for_event();
        if (event.kind == EVENT_TIP) batch[batch_count++] = event.seconds;
        if (batch_count == LOG_BUFFER_LENGTH || event.kind == EVENT_FLUSH) {
            unsigned count = batch_count;
            if (!log_write_batch(chip_id, batch, count, RAINUS_BUILD_UNIX_TIME))
                stop_with_error(ERROR_SD_WRITE);
            batch_count = 0;
#if RAINUS_DEBUG_USB
            saved_count += count;
#endif
            // The marker follows all tips this acknowledgment covers. Tips
            // queued after it remain in the next batch; this isn't a shutdown.
            if (event.kind == EVENT_FLUSH) acknowledge_flush();
        }
    }
}
