#include "stm32f446xx.h"

static const uint16_t AHB_PreScaler[8] =
{
        2,4,8,16,64,128,256,512
};

static const uint8_t APB_PreScaler[4] =
{
        2,4,8,16
};
uint32_t RCC_GetPCLK1Value(void)
{
    uint32_t SystemClk = 16000000;
    uint32_t HCLK;
    uint32_t PCLK1;

    uint8_t clk_src;
    uint8_t ahbprescaler;
    uint8_t apb1prescaler;

    /* Read System Clock Source */
    clk_src = (RCC->CFGR >> 2) & 0x3;

    switch(clk_src)
    {
        case 0:
            SystemClk = 16000000;      // HSI
            break;

        case 1:
            SystemClk = 8000000;       // HSE
            break;

        case 2:
            /* PLL - we'll implement this later */
            SystemClk = 16000000;
            break;
    }

    /* AHB Prescaler */
    ahbprescaler = (RCC->CFGR >> 4) & 0xF;

    if(ahbprescaler < 8)
    {
        HCLK = SystemClk;
    }
    else
    {
        HCLK = SystemClk / AHB_PreScaler[ahbprescaler - 8];
    }

    /* APB1 Prescaler */
    apb1prescaler = (RCC->CFGR >> 10) & 0x7;

    if(apb1prescaler < 4)
    {
        PCLK1 = HCLK;
    }
    else
    {
        PCLK1 = HCLK / APB_PreScaler[apb1prescaler - 4];
    }

    return PCLK1;
}
uint32_t RCC_GetPCLK2Value(void)
{
    uint32_t SystemClk = 16000000;
    uint32_t HCLK;
    uint32_t PCLK2;

    uint8_t clk_src;
    uint8_t ahbprescaler;
    uint8_t apb2prescaler;

    /* Read System Clock Source */
    clk_src = (RCC->CFGR >> 2) & 0x3;

    switch(clk_src)
    {
        case 0:
            SystemClk = 16000000;      // HSI
            break;

        case 1:
            SystemClk = 8000000;       // HSE
            break;

        case 2:
            /* PLL - implement later */
            SystemClk = 16000000;
            break;
    }

    /* AHB Prescaler */
    ahbprescaler = (RCC->CFGR >> 4) & 0xF;

    if(ahbprescaler < 8)
    {
        HCLK = SystemClk;
    }
    else
    {
        HCLK = SystemClk / AHB_PreScaler[ahbprescaler - 8];
    }

    /* APB2 Prescaler */
    apb2prescaler = (RCC->CFGR >> 13) & 0x7;

    if(apb2prescaler < 4)
    {
        PCLK2 = HCLK;
    }
    else
    {
        PCLK2 = HCLK / APB_PreScaler[apb2prescaler - 4];
    }

    return PCLK2;
}
