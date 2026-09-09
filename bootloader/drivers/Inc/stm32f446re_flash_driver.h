#ifndef INC_STM32F446RE_FLASH_DRIVER_H_
#define INC_STM32F446RE_FLASH_DRIVER_H_

#include "stm32f446xx.h"

/* Flash Keys */
#define FLASH_KEY1        0x45670123U
#define FLASH_KEY2        0xCDEF89ABU



#define FLASH_ERASE_SUCCESS    0x00
#define FLASH_ERASE_FAIL       0x01
#define FLASH_WRITE_SUCCESS    0x00
#define FLASH_WRITE_FAIL       0x01
/* APIs */

void FLASH_Unlock(void);
void FLASH_Lock(void);

uint8_t FLASH_WaitForLastOperation(void);

void FLASH_EraseSector(uint8_t SectorNo);

void FLASH_ProgramWord(uint32_t Address,uint32_t Data);
void FLASH_ProgramByte(uint32_t Address,uint8_t Data);
uint8_t FLASH_ProgramBuffer(uint32_t Address, uint8_t *pData, uint32_t Length);
uint32_t FLASH_ReadWord(uint32_t Address);
#endif
