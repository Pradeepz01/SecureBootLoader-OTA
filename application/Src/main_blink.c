/**
 ******************************************************************************
 * @file           : main_blink.c
 * @brief          : Application 1 - Autonomous Periodic Blinking LED (PA5)
 *                   Compatible with STM32F446RE Nucleo-64 & Secure Bootloader
 ******************************************************************************
 */

#include <stdint.h>
#include "systick.h"

/* Peripheral Base Addresses */
#define RCC_BASE      0x40023800U
#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define GPIOA_BASE    0x40020000U
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR     (*(volatile uint32_t *)(GPIOA_BASE + 0x14))

#ifndef BLINK_DELAY_MS
#define BLINK_DELAY_MS 250U /* 250 ms toggle interval (4 Hz toggle / 2 Hz cycle) */
#endif

int main(void)
{
    /* 1. Enable GPIOA Peripheral Clock (Bit 0) */
    RCC_AHB1ENR |= (1 << 0);

    /* 2. Configure PA5 as General Purpose Output (bits 11:10 = 01) */
    GPIOA_MODER &= ~(3 << (5 * 2));
    GPIOA_MODER |=  (1 << (5 * 2));

    /* 3. Initialize SysTick Timer for 1 ms tick interrupts */
    SysTick_Init();

    /* 4. Continuous Periodic Blink Loop */
    while (1)
    {
        if (SysTick_ms_delay(BLINK_DELAY_MS))
        {
            GPIOA_ODR ^= (1 << 5); /* Toggle Green User LED (LD2 / PA5) */
        }
    }

    return 0;
}
