/*
 * stm32f446re_gpio_driver.c
 *
 *  Created on: Jul 7, 2026
 *      Author: HP
 */
#include "stm32f446re_gpio_driver.h"

#include <stdint.h>
#include <stddef.h>
void GPIO_PeriClockControl(GPIO_RegDef_t *pGPIOx, uint8_t EnorDi)
{
    if (EnorDi == ENABLE)
    {
        if (pGPIOx == GPIOA)
        {
            GPIOA_PCLK_EN();
        }
        else if (pGPIOx == GPIOB)
        {
            GPIOB_PCLK_EN();
        }
        else if (pGPIOx == GPIOC)
        {
            GPIOC_PCLK_EN();
        }
        else if (pGPIOx == GPIOD)
        {
            GPIOD_PCLK_EN();
        }
        else if (pGPIOx == GPIOE)
        {
            GPIOE_PCLK_EN();
        }
        else if (pGPIOx == GPIOF)
        {
            GPIOF_PCLK_EN();
        }
        else if (pGPIOx == GPIOG)
        {
            GPIOG_PCLK_EN();
        }
        else if (pGPIOx == GPIOH)
        {
            GPIOH_PCLK_EN();
        }
    }
    else
    {
        if (pGPIOx == GPIOA)
        {
            GPIOA_PCLK_DI();
        }
        else if (pGPIOx == GPIOB)
        {
            GPIOB_PCLK_DI();
        }
        else if (pGPIOx == GPIOC)
        {
            GPIOC_PCLK_DI();
        }
        else if (pGPIOx == GPIOD)
        {
            GPIOD_PCLK_DI();
        }
        else if (pGPIOx == GPIOE)
        {
            GPIOE_PCLK_DI();
        }
        else if (pGPIOx == GPIOF)
        {
            GPIOF_PCLK_DI();
        }
        else if (pGPIOx == GPIOG)
        {
            GPIOG_PCLK_DI();
        }
        else if (pGPIOx == GPIOH)
        {
            GPIOH_PCLK_DI();
        }
    }
}





void GPIO_Init(GPIO_Handle_t *pGPIOHandle){
	    uint32_t temp = 0;
	    GPIO_PeriClockControl(pGPIOHandle->pGPIOx, ENABLE);
	    /**********************************************************************
	     * 1. Configure the GPIO pin mode
	     *********************************************************************/
	    if (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode <= GPIO_MODE_ANALOG)
	    {
	        // Non-interrupt mode

	        temp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode
	                << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));

	        pGPIOHandle->pGPIOx->MODER &=
	            ~(0x3 << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber)); // Clear

	        pGPIOHandle->pGPIOx->MODER |= temp; // Set
	    }
	    else
	    {
	        // Interrupt mode
	        // Configure EXTI registers later
	    	if(pGPIOHandle->GPIO_PinConfig.GPIO_PinMode==GPIO_MODE_IT_FT){
	    			EXTI->FTSR |= (1<< pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
	    			EXTI->RTSR &= ~(1<< pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);

	    	}
	    	else if(pGPIOHandle->GPIO_PinConfig.GPIO_PinMode==GPIO_MODE_IT_RT){
	    			EXTI->RTSR |= (1<< pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
					EXTI->FTSR &= ~(1<< pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
	    	}
	    	else if (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode==GPIO_MODE_IT_RFT){
	    			EXTI->FTSR |= (1<< pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
					EXTI->RTSR |= (1<< pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
	    	}

	    	/*SYSCFG CONFIG FOR SPECIFIC GPIO PORT*/
	    	uint8_t temp1=pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber / 4;
	    	uint8_t temp2=pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber  % 4;
	    	uint8_t portcode= GPIO_BASEADDR_TO_CODE(pGPIOHandle->pGPIOx);
	    	SYSCFG_PCLK_EN();
	    	SYSCFG->EXTICR[temp1]=portcode << (temp2*4);


	    	EXTI->IMR |=(1<< pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
	    }

	    /**********************************************************************
	     * 2. Configure the GPIO output speed
	     *********************************************************************/
	    temp = 0;

	    temp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinSpeed
	            << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));

	    pGPIOHandle->pGPIOx->OSPEEDR &=
	        ~(0x3 << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber)); // Clear

	    pGPIOHandle->pGPIOx->OSPEEDR |= temp; // Set

	    /**********************************************************************
	     * 3. Configure the GPIO pull-up/pull-down
	     *********************************************************************/
	    temp = 0;

	    temp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinPuPdControl
	            << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));

	    pGPIOHandle->pGPIOx->PUPDR &=
	        ~(0x3 << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber)); // Clear

	    pGPIOHandle->pGPIOx->PUPDR |= temp; // Set

	    /**********************************************************************
	     * 4. Configure the GPIO output type
	     *********************************************************************/
	    temp = 0;

	    temp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinOPType
	            << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);

	    pGPIOHandle->pGPIOx->OTYPER &=
	        ~(0x1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber); // Clear

	    pGPIOHandle->pGPIOx->OTYPER |= temp; // Set

	    /**********************************************************************
	     * 5. Configure the GPIO alternate function
	     *********************************************************************/
	    if (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_ALTFN)
	    {
	        uint8_t temp1, temp2;

	        temp1 = pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber / 8;
	        temp2 = pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber % 8;

	        pGPIOHandle->pGPIOx->AFR[temp1] &=
	            ~(0xF << (4 * temp2)); // Clear

	        pGPIOHandle->pGPIOx->AFR[temp1] |=
	            (pGPIOHandle->GPIO_PinConfig.GPIO_PinAltFunMode
	             << (4 * temp2)); // Set
	    }


}
void GPIO_DeInit(GPIO_RegDef_t *pGPIOx){
			if (pGPIOx == GPIOA)
			{
				GPIOA_REG_RESET();
			}
			else if (pGPIOx == GPIOB)
			{
				GPIOB_REG_RESET();
			}
			else if (pGPIOx == GPIOC)
			{
				GPIOC_REG_RESET();
			}
			else if (pGPIOx == GPIOD)
			{
				GPIOD_REG_RESET();
			}
			else if (pGPIOx == GPIOE)
			{
				GPIOE_REG_RESET();
			}
			else if (pGPIOx == GPIOF)
			{
				GPIOF_REG_RESET();
			}
			else if (pGPIOx == GPIOG)
			{
				GPIOG_REG_RESET();
			}
			else if (pGPIOx == GPIOH)
			{
				GPIOH_REG_RESET();
			}
}

uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber){
	uint8_t value;
	value = (pGPIOx->IDR >> PinNumber & 0x00001);
	return value;
}
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t *pGPIOx){
	uint16_t value;
	value =(pGPIOx->IDR);
	return value;
}

void GPIO_WriteToOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber, uint8_t Value){
	pGPIOx->ODR &= ~(1U<<PinNumber);
	pGPIOx->ODR |=  (Value<<PinNumber);
}
void GPIO_WriteToOutputPort(GPIO_RegDef_t *pGPIOx, uint16_t Value){
	pGPIOx->ODR =Value;
}

void GPIO_ToggleOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber){
	pGPIOx->ODR ^= (1U<<PinNumber);
}

void GPIO_IRQConfig(uint8_t IRQNumber, uint8_t EnorDi){
	if(EnorDi==ENABLE){
		if(IRQNumber <=31){
			*NVIC_ISER0 |=(1<< IRQNumber);
		}
		else if(IRQNumber>31 && IRQNumber <64){
			*NVIC_ISER1 |=(1<<IRQNumber % 32);
		}
		else if(IRQNumber>=64 && IRQNumber <96){
			*NVIC_ISER1 |=(1<<IRQNumber % 32);
		}
	  }
	else{
		if(IRQNumber <=31){
			*NVIC_ICER0 |=(1<< IRQNumber);
		}
		else if(IRQNumber>31 && IRQNumber <64){
			*NVIC_ICER1 |=(1<<IRQNumber % 32);
		}
		else if(IRQNumber>=64 && IRQNumber <96){
			*NVIC_ICER1 |=(1<<IRQNumber % 32);
	}


	}
}
void GPIO_IRQPriority(uint8_t IRQNumber,uint8_t IRQPriority){
	uint8_t iprx= IRQNumber /4;
	uint8_t iprx_section= IRQNumber %4;
	*(NVIC_PR_BASE_ADDR + (iprx *4)) |= (IRQPriority << ((8 * iprx_section) + 4));
}
void GPIO_IRQHandling(uint8_t PinNumber){
	if(EXTI->PR & (1<<PinNumber)){
		EXTI->PR|= (1<< PinNumber);
	}
}


