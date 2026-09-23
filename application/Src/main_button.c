/**
 ******************************************************************************
 * @file           : main_button.c
 * @brief          : Application 2 - Push-Button Controlled LED
 *                   Hardware: STM32F446RE Nucleo-64
 *                   LED (LD2)  : PA5 (Green LED, Active High)
 *                   Button (B1): PC13 (Blue User Button, Active Low)
 ******************************************************************************
 */

#include <stdint.h>

/* Peripheral Base Addresses */
#define RCC_BASE      0x40023800U
#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define GPIOA_BASE    0x40020000U
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR     (*(volatile uint32_t *)(GPIOA_BASE + 0x14))

#define GPIOC_BASE    0x40020800U
#define GPIOC_MODER   (*(volatile uint32_t *)(GPIOC_BASE + 0x00))
#define GPIOC_PUPDR   (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))
#define GPIOC_IDR     (*(volatile uint32_t *)(GPIOC_BASE + 0x10))

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles--)
    {
        __asm volatile ("nop");
    }
}

int main(void)
{
    /* 1. Enable Peripheral Clocks: GPIOA (bit 0) and GPIOC (bit 2) */
    RCC_AHB1ENR |= (1 << 0) | (1 << 2);

    /* 2. Configure PA5 as General Purpose Output (bits 11:10 = 01) */
    GPIOA_MODER &= ~(3 << (5 * 2));
    GPIOA_MODER |=  (1 << (5 * 2));

    /* 3. Configure PC13 as Input (bits 27:26 = 00) with Pull-Up (bits 27:26 = 01) */
    GPIOC_MODER &= ~(3 << (13 * 2));
    GPIOC_PUPDR &= ~(3 << (13 * 2));
    GPIOC_PUPDR |=  (1 << (13 * 2));

    /* 4. Startup Indicator: 3 rapid greeting pulses to confirm Button App booted */
    for (int i = 0; i < 3; i++)
    {
        GPIOA_ODR |= (1 << 5);
        delay_cycles(400000);
        GPIOA_ODR &= ~(1 << 5);
        delay_cycles(400000);
    }

    /* 5. Main Loop: Button controls LED
     *    On Nucleo-64:
     *    - Button B1 pressed  -> PC13 is LOW (0)  -> LED PA5 ON
     *    - Button B1 released -> PC13 is HIGH (1) -> LED PA5 OFF
     */
    while (1)
    {
        if ((GPIOC_IDR & (1 << 13)) == 0)
        {
            /* Button is PRESSED -> LED ON */
            GPIOA_ODR |= (1 << 5);
        }
        else
        {
            /* Button is RELEASED -> LED OFF */
            GPIOA_ODR &= ~(1 << 5);
        }
    }

    return 0;
}
