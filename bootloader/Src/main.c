#include "bootloader.h"

#include "stm32f446re_gpio_driver.h"
#include "stm32f446re_usart_driver.h"
#include "stm32f446re_flash_driver.h"
#include <string.h>

void delay(void){
	for(uint32_t i = 0; i<5000000 ;i++);
}

int main(void)
{

	GPIO_Handle_t Led;

	Led.pGPIOx = GPIOA;
	Led.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_5;
	Led.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_OUT;
	Led.GPIO_PinConfig.GPIO_PinOPType = GPIO_OP_TYPE_PP;
	Led.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_NO_PUPD;
	Led.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_FAST;

	GPIO_Init(&Led);


    GPIO_Handle_t USARTPins;

    USARTPins.pGPIOx = GPIOA;

    USARTPins.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ALTFN;
    USARTPins.GPIO_PinConfig.GPIO_PinOPType = GPIO_OP_TYPE_PP;
    USARTPins.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_PIN_PU;
    USARTPins.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_FAST;
    USARTPins.GPIO_PinConfig.GPIO_PinAltFunMode = 7;

    /* PA2 -> TX */
    USARTPins.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_2;
    GPIO_Init(&USARTPins);

    /* PA3 -> RX */
    USARTPins.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_3;
    GPIO_Init(&USARTPins);



    USART_Handle_t USART2Handle;

    USART2Handle.pUSARTx = USART2;

    USART2Handle.USART_Config.USART_Baud = USART_STD_BAUD_115200;
    USART2Handle.USART_Config.USART_Mode = USART_MODE_TXRX;
    USART2Handle.USART_Config.USART_NoOfStopBits = USART_STOPBITS_1;
    USART2Handle.USART_Config.USART_WordLength = USART_WORDLEN_8BITS;
    USART2Handle.USART_Config.USART_ParityControl = USART_PARITY_DISABLE;

	USART_Init(&USART2Handle);
	/*
	#define TEST_FLASH_ADDRESS    0x08010000U

	uint32_t ReadData;
	uint32_t WriteData = 0x12345678;
	char msg[50];

	FLASH_Unlock();


	FLASH_EraseSector(5);

	FLASH_ProgramWord(TEST_FLASH_ADDRESS, WriteData);

	ReadData = FLASH_ReadWord(TEST_FLASH_ADDRESS);

	FLASH_Lock();

	if(ReadData == WriteData)
	{
		sprintf(msg, "FLASH TEST PASSED\r\n");
	}
	else
	{
		sprintf(msg, "FLASH TEST FAILED\r\n");
	}

	USART_SendData(USART2Handle.pUSARTx, (uint8_t *)msg, strlen(msg));  */

	CRC_PCLK_EN();

	/* Initialize / restore slot metadata table */
	SlotTable_Init();

	/* Indicate bootloader is ready (LED ON) */
	GPIO_WriteToOutputPin(GPIOA, GPIO_PIN_NO_5, 1);

	while(1)
	{
		Bootloader_ReadCommand();
	}

    /* Reaches here only if the application is invalid
       or the jump somehow returns */
}
