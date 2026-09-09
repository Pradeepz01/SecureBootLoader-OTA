/*
 * stm32f446re_usart_driver.c
 *
 *  Created on: Jul 25, 2026
 *      Author: HP
 */
#include "stm32f446re_usart_driver.h"


#define USART_CR1_UE    13
void USART_PeriClockControl(USART_RegDef_t *pUSARTx, uint8_t EnOrDi)
{
    if (EnOrDi == ENABLE)
    {
        if (pUSARTx == USART1)
        {
            USART1_PCLK_EN();
        }
        else if (pUSARTx == USART2)
        {
            USART2_PCLK_EN();
        }
        else if (pUSARTx == USART6)
        {
            USART6_PCLK_EN();
        }
    }
    else
    {
        if (pUSARTx == USART1)
        {
            USART1_PCLK_DI();
        }
        else if (pUSARTx == USART2)
        {
            USART2_PCLK_DI();
        }
        else if (pUSARTx == USART6)
        {
            USART6_PCLK_DI();
        }
    }
}
void USART_Init(USART_Handle_t *pUSARTHandle)
{
    uint32_t tempreg = 0;

    USART_PeriClockControl(pUSARTHandle->pUSARTx, ENABLE);

    /* Mode */

    if(pUSARTHandle->USART_Config.USART_Mode == USART_MODE_ONLY_RX)
        tempreg |= (1 << 2);

    else if(pUSARTHandle->USART_Config.USART_Mode == USART_MODE_ONLY_TX)
        tempreg |= (1 << 3);

    else if(pUSARTHandle->USART_Config.USART_Mode == USART_MODE_TXRX)
    {
        tempreg |= (1 << 2);
        tempreg |= (1 << 3);
    }

    /* Word Length */

    if(pUSARTHandle->USART_Config.USART_WordLength == USART_WORDLEN_9BITS)
        tempreg |= (1 << 12);


    /* Configure Parity */

    if(pUSARTHandle->USART_Config.USART_ParityControl == USART_PARITY_EN_EVEN)
    {
        tempreg |= (1U << USART_CR1_PCE);
    }
    else if(pUSARTHandle->USART_Config.USART_ParityControl == USART_PARITY_EN_ODD)
    {
        tempreg |= (1U << USART_CR1_PCE);
        tempreg |= (1U << USART_CR1_PS);
    }
    pUSARTHandle->pUSARTx->CR1 = tempreg;
    tempreg = 0;

    tempreg |= (pUSARTHandle->USART_Config.USART_NoOfStopBits << USART_CR2_STOP);

    pUSARTHandle->pUSARTx->CR2 = tempreg;
    /* Configure baud rate */
    USART_SetBaudRate(pUSARTHandle->pUSARTx,
                      pUSARTHandle->USART_Config.USART_Baud);

    /* Enable USART peripheral */
    pUSARTHandle->pUSARTx->CR1 |= (1 << USART_CR1_UE);

}
void USART_SetBaudRate(USART_RegDef_t *pUSARTx, uint32_t BaudRate)
{
    uint32_t PCLKx;
    uint32_t usartdiv;
    uint32_t M_part;
    uint32_t F_part;
    uint32_t tempreg = 0;

    /* Get the peripheral clock */

    if(pUSARTx == USART1 || pUSARTx == USART6)
    {
        PCLKx = RCC_GetPCLK2Value();
    }
    else
    {
        PCLKx = RCC_GetPCLK1Value();
    }
    /* Calculate USARTDIV multiplied by 100 */

    usartdiv = (25U * PCLKx) / (4U * BaudRate);
    /* Extract mantissa */

    M_part = usartdiv / 100U;

    tempreg |= (M_part << 4);
    /* Fraction */
   F_part = usartdiv %100U;

   F_part = ((F_part * 16U) + 50U) / 100U;

   tempreg |= (F_part & 0x0F);

   /* Program BRR Register */
   pUSARTx->BRR = tempreg;
}
uint8_t USART_GetFlagStatus(USART_RegDef_t *pUSARTx, uint32_t FlagName)
{
    if(pUSARTx->SR & FlagName)
    {
        return FLAG_SET;
    }

    return FLAG_RESET;
}
void USART_SendData(USART_RegDef_t *pUSARTx, uint8_t *pTxBuffer, uint32_t Len)
{
    while(Len > 0)
    {
        /* Wait until TXE is set */
        while(USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == FLAG_RESET);

        /* Check word length */
        if(pUSARTx->CR1 & (1 << USART_CR1_M))
        {
            /* 9-bit data frame */

            pUSARTx->DR = (*(uint16_t *)pTxBuffer & 0x01FF);

            if(!(pUSARTx->CR1 & (1 << USART_CR1_PCE)))
            {
                pTxBuffer += 2;
                Len -= 2;
            }
            else
            {
                pTxBuffer++;
                Len--;
            }
        }
        else
        {
            /* 8-bit data frame */

            pUSARTx->DR = (*pTxBuffer & 0xFF);

            pTxBuffer++;
            Len--;
        }
    }

    /* Wait until transmission is complete */
    while(USART_GetFlagStatus(pUSARTx, USART_FLAG_TC) == FLAG_RESET);
}
void USART_ReceiveData(USART_RegDef_t *pUSARTx, uint8_t *pRxBuffer, uint32_t Len){
    while(Len > 0)
    {
        /* Wait until RXNE flag is set */
        while(USART_GetFlagStatus(pUSARTx, USART_FLAG_RXNE) == FLAG_RESET);

        /* Check the word length */
        if(pUSARTx->CR1 & (1U << USART_CR1_M))
        {
            /* 9-bit frame */

            if(!(pUSARTx->CR1 & (1U << USART_CR1_PCE)))
            {
                /* No parity */

                *((uint16_t *)pRxBuffer) = (pUSARTx->DR & 0x01FF);

                pRxBuffer += 2;
                Len -= 2;
            }
            else
            {
                /* Parity enabled */

                *pRxBuffer = (uint8_t)(pUSARTx->DR & 0x00FF);

                pRxBuffer++;
                Len--;
            }
        }
        else
        {
            /* 8-bit frame */

            if(!(pUSARTx->CR1 & (1U << USART_CR1_PCE)))
            {
                /* No parity */

                *pRxBuffer = (uint8_t)(pUSARTx->DR & 0x00FF);
            }
            else
            {
                /* Parity enabled */

                *pRxBuffer = (uint8_t)(pUSARTx->DR & 0x007F);
            }

            pRxBuffer++;
            Len--;
        }
    }
}
void USART_SendString(USART_RegDef_t *pUSARTx, char *str)
{
    while(*str)
    {
        USART_SendData(pUSARTx, (uint8_t *)str, 1);
        str++;
    }
}
void USART_SendHexByte(USART_RegDef_t *pUSARTx, uint8_t data)
{
    char hex[3];

    const char table[] = "0123456789ABCDEF";

    hex[0] = table[(data >> 4) & 0x0F];
    hex[1] = table[data & 0x0F];
    hex[2] = '\0';

    USART_SendString(pUSARTx, hex);
}
