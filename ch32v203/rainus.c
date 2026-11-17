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
#define BATCH_SIZE 10
#define QUEUE_SIZE 32 // Holds up to 31 edges while foreground code is busy.

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
// The interrupt captures events even while the main loop is writing to SD.
// One slot stays empty: equal read/write indexes mean the queue is empty.
// volatile makes the compiler reload shared state; wait_for_event() masks
// interrupts while removing an item. volatile alone is not a lock.
static volatile uint32_t event_queue[QUEUE_SIZE];
static volatile unsigned queue_write_index, queue_read_index;
static volatile int queue_overflow;
static volatile clock_result_t clock_fault;
static debounce_t debounce;
// The batch serves a different purpose from the queue: group 10 events into
// one SD operation to save energy. Its contents stay fixed during that write,
// while the interrupt can still add newly arriving events to event_queue.
// All this RAM is lost on reset or power loss. Static variables start at zero.
static uint32_t batch[BATCH_SIZE];
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

// Terminal error policy: stop accepting switch events and repeat the error
// code until reset. LED delays block here; USB still runs between groups.
// We do not write a sensor record with an untrusted RTC or retry a partial
// SD batch (which could duplicate rows that were already saved).
static void stop_with_error(logger_error_t error)
{
    NVIC_DisableIRQ(EXTI0_IRQn);
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

// This interrupt also runs during SD writes. It captures the time and queues
// it; slower filesystem operations happen in the main loop.
__attribute__((interrupt)) void EXTI0_IRQHandler(void)
{
    if (!(EXTI->INTFR & EXTI_Line0)) return;
    EXTI->INTFR = EXTI_Line0;
#if RAINUS_DEBUG_USB
    ++interrupt_count;
#endif
    uint64_t ticks;
    if (!clock_ticks(&ticks)) {
        clock_fault = CLOCK_SYNC_TIMEOUT;
        return;
    }
    debounce_result_t result = debounce_edge(&debounce, ticks);
    if (result == DEBOUNCE_CLOCK_BACKWARDS) {
        clock_fault = CLOCK_MOVED_BACKWARDS;
        return;
    }
    if (result == DEBOUNCE_IGNORED) return;
    unsigned next = (queue_write_index + 1) % QUEUE_SIZE;
    if (next == queue_read_index) {
        queue_overflow = 1;
        return;
    }
    // Divide 32768-Hz ticks by 32768 (2^15) for whole Unix seconds.
    event_queue[queue_write_index] = ticks >> 15;
    queue_write_index = next;
}

static void enable_switch_interrupt(void)
{
    // A dry contact from PA0 to GND pulls the normally-high pin LOW.
    funPinMode(PA0, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA0, FUN_HIGH);
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
    EXTI->RTENR &= ~EXTI_Line0;
    EXTI->FTENR |= EXTI_Line0;
    EXTI->INTFR = EXTI_Line0;
    EXTI->INTENR |= EXTI_Line0;
    // Also latch an event for WFE, even while interrupts are masked.
    EXTI->EVENR |= EXTI_Line0;
    NVIC_EnableIRQ(EXTI0_IRQn);
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
// WFE = Wait For Event. EXTI latches a PA0 edge as a hardware event even with
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
    if (!(EXTI->INTFR & EXTI_Line0))
        __asm__ volatile ("wfi" ::: "memory");
    NVIC->SCTLR &= ~(PFIC_SLEEP_DEEP | PFIC_WFI_AS_WFE);
}
#endif

// Block until one accepted timestamp is available, or stop on a fault.
// In the field build an empty queue leads to STOP, not periodic pin polling.
// A wake can occur without a queued timestamp (e.g. a filtered bounce), so
// check again after waking. Return with interrupts enabled before any SD work.
static uint32_t wait_for_event(void)
{
    for (;;) {
        service_debug();
        __disable_irq();
        compiler_barrier();

        if (clock_fault || queue_overflow) {
            logger_error_t error = clock_fault ? ERROR_RTC : ERROR_QUEUE_FULL;
#if RAINUS_DEBUG_USB
            if (clock_fault) clock_error = clock_fault;
#endif
            compiler_barrier();
            __enable_irq();
            stop_with_error(error);
        }
        if (queue_read_index != queue_write_index) {
            uint32_t seconds = event_queue[queue_read_index];
            queue_read_index = (queue_read_index + 1) % QUEUE_SIZE;
            compiler_barrier();
            __enable_irq();
            return seconds;
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
    // Events can be captured while the SD card mounts. They wait in the queue
    // until the main loop runs; an overflow stops the logger with error 4.
    enable_switch_interrupt();
    if (f_mount(&filesystem, "", 1) != FR_OK) stop_with_error(ERROR_SD_MOUNT);

    // Initialization above runs once per reset. This loop runs for the whole
    // deployment: wait/asleep -> collect timestamp -> write every 10 events.
    // The SD write blocks this loop, but the switch interrupt stays enabled.
    for (;;) {
        uint32_t seconds = wait_for_event();
        batch[batch_count++] = seconds;
        if (batch_count == BATCH_SIZE) {
            if (!log_write_batch(chip_id, batch, BATCH_SIZE, RAINUS_BUILD_UNIX_TIME))
                stop_with_error(ERROR_SD_WRITE);
            batch_count = 0;
#if RAINUS_DEBUG_USB
            saved_count += BATCH_SIZE;
#endif
        }
    }
}
