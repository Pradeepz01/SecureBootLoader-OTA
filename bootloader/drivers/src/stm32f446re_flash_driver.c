/*
 * stm32f446re_flash_driver.c
 *
 *  Created on: Jul 26, 2026
 *      Author: HP
 */
#include "stm32f446re_flash_driver.h"

void FLASH_Unlock(void)
{
    if(FLASH_REGS->CR & (1 << FLASH_CR_LOCK))
    {
        FLASH_REGS->KEYR = FLASH_KEY1;
        FLASH_REGS->KEYR = FLASH_KEY2;
    }
}
void FLASH_Lock(void)
{
    FLASH_REGS->CR |= (1 << FLASH_CR_LOCK);
}

uint8_t FLASH_WaitForLastOperation(void)
{
    while (FLASH_REGS->SR & (1 << FLASH_SR_BSY));

    /* Check for errors */
    if (FLASH_REGS->SR & ((1 << FLASH_SR_OPERR) |
                          (1 << FLASH_SR_WRPERR) |
                          (1 << FLASH_SR_PGAERR) |
                          (1 << FLASH_SR_PGPERR) |
                          (1 << FLASH_SR_PGSERR)))
    {
        /* Clear error flags */
        FLASH_REGS->SR = (1 << FLASH_SR_OPERR) |
                         (1 << FLASH_SR_WRPERR) |
                         (1 << FLASH_SR_PGAERR) |
                         (1 << FLASH_SR_PGPERR) |
                         (1 << FLASH_SR_PGSERR);
        return 0;   /* Error */
    }

    return 1;       /* Success */
}
void FLASH_EraseSector(uint8_t SectorNo)
{
    /* Clear any pending error flags before starting */
    FLASH_REGS->SR = (1 << FLASH_SR_OPERR) |
                     (1 << FLASH_SR_WRPERR) |
                     (1 << FLASH_SR_PGAERR) |
                     (1 << FLASH_SR_PGPERR) |
                     (1 << FLASH_SR_PGSERR);

    FLASH_WaitForLastOperation();

    /* Configure parallelism = x32 */
    FLASH_REGS->CR &= ~(0x3U << FLASH_CR_PSIZE);
    FLASH_REGS->CR |=  (0x2U << FLASH_CR_PSIZE);

    /* Clear previous sector selection */
    FLASH_REGS->CR &= ~(0xFU << FLASH_CR_SNB);

    /* Sector Erase */
    FLASH_REGS->CR |= (1 << FLASH_CR_SER);

    /* Select sector */
    FLASH_REGS->CR |= (SectorNo << FLASH_CR_SNB);

    /* Start erase */
    FLASH_REGS->CR |= (1 << FLASH_CR_STRT);

    /* Wait */
    FLASH_WaitForLastOperation();

    /* Disable erase */
    FLASH_REGS->CR &= ~(1 << FLASH_CR_SER);
}
void FLASH_ProgramWord(uint32_t Address, uint32_t Data)
{
    /* Wait if Flash is busy */
    FLASH_WaitForLastOperation();

    /* Configure Programming Size = 32 bits */

    FLASH_REGS->CR &= ~(0x3U << FLASH_CR_PSIZE);   // Clear PSIZE bits
    FLASH_REGS->CR |=  (0x2U << FLASH_CR_PSIZE);   // 10 = x32 programming

    /* Enable Programming */
    FLASH_REGS->CR |= (1 << FLASH_CR_PG);

    /* Write Data */
    *(volatile uint32_t *)Address = Data;

    /* Wait for programming to finish */
    FLASH_WaitForLastOperation();

    /* Disable Programming */
    FLASH_REGS->CR &= ~(1 << FLASH_CR_PG);
}
void FLASH_ProgramByte(uint32_t Address,uint8_t Data)
{
    FLASH_WaitForLastOperation();

    FLASH_REGS->CR &= ~(3 << FLASH_CR_PSIZE);

    /* x8 Programming */

    FLASH_REGS->CR |= (0 << FLASH_CR_PSIZE);

    FLASH_REGS->CR |= (1 << FLASH_CR_PG);

    *(volatile uint8_t *)Address = Data;

    FLASH_WaitForLastOperation();

    FLASH_REGS->CR &= ~(1 << FLASH_CR_PG);
}
uint32_t FLASH_ReadWord(uint32_t Address)
{
    return *(volatile uint32_t *)Address;
}

uint8_t FLASH_ProgramBuffer(uint32_t Address, uint8_t *pData, uint32_t Length)
{
    FLASH_Unlock();
    for (uint32_t i = 0; i < Length; i++)
    {
        FLASH_ProgramByte(Address + i, pData[i]);
        if (*(volatile uint8_t *)(Address + i) != pData[i])
        {
            FLASH_Lock();
            return FLASH_WRITE_FAIL;
        }
    }
    FLASH_Lock();
    return FLASH_WRITE_SUCCESS;
}
