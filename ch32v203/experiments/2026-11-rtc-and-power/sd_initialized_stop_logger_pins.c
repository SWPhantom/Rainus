// Board power-floor experiment. This is NOT the Rainus logger.
// It initializes the SD card, stops the RTC, and never logs events.
#include "ch32fun.h"
#include "../fatfs/diskio.h"

int main(void)
{
    SystemInit();
    funGpioInitAll();

    // The first flash shows startup; the second confirms SD initialization.
    // The LED is off
    // during the measurement; the normal USB bootloader remains available.
    funDigitalWrite(PA15, FUN_HIGH);
    funPinMode(PA15, GPIO_CFGLR_OUT_10Mhz_PP);
    funDigitalWrite(PA15, FUN_LOW);
    Delay_Ms(100);
    funDigitalWrite(PA15, FUN_HIGH);

    // Initialize the card into SPI mode before entering STOP mode.
    // Three repeating flashes mean initialization failed; never report this
    // error loop as a valid STOP measurement.
    if (disk_initialize(0) & STA_NOINIT) {
        for (;;) {
            for (int i = 0; i < 3; ++i) {
                funDigitalWrite(PA15, FUN_LOW); Delay_Ms(150);
                funDigitalWrite(PA15, FUN_HIGH); Delay_Ms(200);
            }
            Delay_Ms(1200);
        }
    }
    Delay_Ms(300);
    funDigitalWrite(PA15, FUN_LOW); Delay_Ms(100);
    funDigitalWrite(PA15, FUN_HIGH);

    RCC->APB1PCENR |= RCC_APB1Periph_PWR | RCC_APB1Periph_BKP;
    PWR->CTLR |= PWR_CTLR_DBP;
    // Stop the RTC clock and crystal, but don't reset the backup domain.
    // Its timestamp will not advance while this experiment is running.
    RCC->BDCTLR &= ~RCC_RTCEN;
    RCC->BDCTLR &= ~RCC_LSEON;
    RCC->RSTSCKR &= ~RCC_LSION;

    // Match the logger's pin configuration: SD SPI pins remain as set by
    // disk_initialize(), PA15 remains an output with LED off, and PA0 uses
    // its pull-up. Other pins retain their reset configuration. This test
    // intentionally does not enable switch interrupts or the RTC.
    funPinMode(PA0, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA0, FUN_HIGH);

    EXTI->INTENR = 0;
    EXTI->EVENR = 0;
    PWR->CSR &= ~PWR_CSR_EWUP;
    __disable_irq();
    SysTick->CTLR = 0;

    // STOP with the normal regulator, no RTC or wake pin. Only a reset/power
    // cycle restarts this test; it immediately returns to STOP afterward.
    PWR->CTLR = PWR_CTLR_CWUF | PWR_CTLR_CSBF; // STOP, normal regulator.
    NVIC->SCTLR |= 1 << 2;
    for (;;) __WFI();
}
