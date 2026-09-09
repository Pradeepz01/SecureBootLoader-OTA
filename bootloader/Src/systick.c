#include "systick.h"
#include <stdint.h>
#define SYSTICK_BASE    0xE000E010

#define STK_CTRL    (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define STK_LOAD    (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define STK_VAL     (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))

volatile uint32_t tick = 0;
uint32_t previous = 0;
void SysTick_Init(void)
{
    STK_LOAD = 16000 - 1;      // 1 ms @ 16 MHz
    STK_VAL  = 0;

    STK_CTRL =
        (1<<2) |   // Processor clock
        (1<<1) |   // Interrupt enable
        (1<<0);    // Counter enable
}

void SysTick_Handler(void)
{
    tick++;
}

uint32_t SysTick_ms_delay(uint32_t value)
{
	if ((tick - previous) >= value){
	       previous=tick;
    		return 1;
	}
	return 0;
}
