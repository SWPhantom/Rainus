// Board power-floor experiment. This is NOT the Rainus logger.
// It stops the RTC, does not initialize the SD card, and never logs events.
#include "ch32fun.h"

int main(void)
{
    SystemInit();
    funGpioInitAll();

    // One brief flash shows that this experiment started. The LED is off
    // during the measurement; the normal USB bootloader remains available.
    funDigitalWrite(PA15, FUN_HIGH);
    funPinMode(PA15, GPIO_CFGLR_OUT_10Mhz_PP);
    funDigitalWrite(PA15, FUN_LOW);
    Delay_Ms(100);
    funDigitalWrite(PA15, FUN_HIGH);

    RCC->APB1PCENR |= RCC_APB1Periph_PWR | RCC_APB1Periph_BKP;
    PWR->CTLR |= PWR_CTLR_DBP;
    // Stop the RTC clock and crystal, but don't reset the backup domain.
    // Its timestamp will not advance while this experiment is running.
    RCC->BDCTLR &= ~RCC_RTCEN;
    RCC->BDCTLR &= ~RCC_LSEON;
    RCC->RSTSCKR &= ~RCC_LSION;

    // Disable debug pin overrides, then put GPIOs in analog input mode to
    // avoid floating digital input buffers. No sensor pins will wake the MCU.
    AFIO->PCFR1 = (AFIO->PCFR1 & ~AFIO_PCFR1_SWJ_CFG) | AFIO_PCFR1_SWJ_CFG_DISABLE;
    GPIOA->CFGLR = GPIOA->CFGHR = 0;
    GPIOB->CFGLR = GPIOB->CFGHR = 0;
    GPIOC->CFGLR = GPIOC->CFGHR = 0;
    GPIOD->CFGLR = GPIOD->CFGHR = 0;

    EXTI->INTENR = 0;
    EXTI->EVENR = 0;
    PWR->CSR &= ~PWR_CSR_EWUP;
    __disable_irq();
    SysTick->CTLR = 0;

    // Standby, no RAM retention, no RTC or wake pin. Only a reset/power
    // cycle restarts this test; it immediately returns to standby afterward.
    PWR->CTLR = PWR_CTLR_PDDS | PWR_CTLR_CWUF | PWR_CTLR_CSBF;
    NVIC->SCTLR |= 1 << 2;
    for (;;) __WFI();
}
