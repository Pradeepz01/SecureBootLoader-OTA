/*
 * stm32f446re_spi_driver.c
 *
 *  Created on: Jul 19, 2026
 *      Author: HP
 */
/* SPI Configuration Structure */
#include "stm32f446re_spi_driver.h"


void SPI_PeriClockControl(SPI_RegDef_t *pSPIx, uint8_t EnorDi){
	if (EnorDi == ENABLE)
	{
	    if (pSPIx == SPI1)
	    {
	        SPI1_PCLK_EN();
	    }
	    else if (pSPIx == SPI2)
	    {
	        SPI2_PCLK_EN();
	    }
	    else if (pSPIx == SPI3)
	    {
	        SPI3_PCLK_EN();
	    }
	    else if (pSPIx == SPI4)
		{
			SPI4_PCLK_EN();
		}
	}
	else
	{
	    if (pSPIx == SPI1)
	    {
	        SPI1_PCLK_DI();
	    }
	    else if (pSPIx == SPI2)
	    {
	        SPI2_PCLK_DI();
	    }
	    else if (pSPIx == SPI3)
	    {
	        SPI3_PCLK_DI();
	    }
	    else if (pSPIx == SPI4)
		{
			SPI4_PCLK_DI();
		}
	}
}

void SPI_Init(SPI_Handle_t *pSPIHandle)
{
    uint32_t tempreg = 0;

    /****************************************************
     * 1. Configure Device Mode (MSTR)
     ****************************************************/
    tempreg |= (pSPIHandle->SPIConfig.SPI_DeviceMode << SPI_CR1_MSTR);

    /****************************************************
     * 2. Configure Bus Configuration
     ****************************************************/
    if (pSPIHandle->SPIConfig.SPI_BusConfig == SPI_BUS_CONFIG_FD)
    {
        // BIDIMODE = 0
        tempreg &= ~(1 << SPI_CR1_BIDIMODE);
    }
    else if (pSPIHandle->SPIConfig.SPI_BusConfig == SPI_BUS_CONFIG_HD)
    {
        // BIDIMODE = 1
        tempreg |= (1 << SPI_CR1_BIDIMODE);
    }
    else if (pSPIHandle->SPIConfig.SPI_BusConfig == SPI_BUS_CONFIG_SIMPLEX_RXONLY)
    {
        // BIDIMODE = 0
        tempreg &= ~(1 << SPI_CR1_BIDIMODE);

        // RXONLY = 1
        tempreg |= (1 << SPI_CR1_RXONLY);
    }

    /****************************************************
     * 3. Configure Serial Clock Speed (BR[2:0])
     ****************************************************/
    tempreg |= (pSPIHandle->SPIConfig.SPI_SclkSpeed << SPI_CR1_BR);

    /****************************************************
     * 4. Configure Data Frame Format (DFF)
     ****************************************************/
    tempreg |= (pSPIHandle->SPIConfig.SPI_DFF << SPI_CR1_DFF);

    /****************************************************
     * 5. Configure Clock Polarity (CPOL)
     ****************************************************/
    tempreg |= (pSPIHandle->SPIConfig.SPI_CPOL << SPI_CR1_CPOL);

    /****************************************************
     * 6. Configure Clock Phase (CPHA)
     ****************************************************/
    tempreg |= (pSPIHandle->SPIConfig.SPI_CPHA << SPI_CR1_CPHA);

    /****************************************************
     * 7. Configure Software Slave Management (SSM)
     ****************************************************/
    tempreg |= (pSPIHandle->SPIConfig.SPI_SSM << SPI_CR1_SSM);

    /****************************************************
     * Write the configuration to CR1
     ****************************************************/
    pSPIHandle->pSPIx->CR1 = tempreg;
}
void SPI_DeInit(SPI_RegDef_t *pSPIx)
{
    if (pSPIx == SPI1)
    {
        SPI1_REG_RESET();
    }
    else if (pSPIx == SPI2)
    {
        SPI2_REG_RESET();
    }
    else if (pSPIx == SPI3)
    {
        SPI3_REG_RESET();
    }
    else if (pSPIx == SPI4)
    {
        SPI4_REG_RESET();
    }
}
uint8_t SPI_GetFlagStatus(SPI_RegDef_t *pSPIx ,uint32_t FlagName){
	if(pSPIx->SR & FlagName){
		return FLAG_SET;
	}

	return FLAG_RESET;
}

void SPI_SendData(SPI_RegDef_t *pSPIx,uint8_t *pTXBuffer , uint32_t Len){
	while(Len >0){
		while(SPI_GetFlagStatus(pSPIx,SPI_TXE_FLAG)==FLAG_RESET);
		if(pSPIx->CR1 & (1 << SPI_CR1_DFF)){
			pSPIx->DR =*((uint16_t*)pTXBuffer);
			Len--;
			Len--;
			(uint16_t*)pTXBuffer++;

		}else{
			pSPIx->DR =*pTXBuffer;
			Len--;
			pTXBuffer++;
		}
	}
}
void SPI_ReceiveData(SPI_RegDef_t *pSPIx,uint8_t *pRXBuffer , uint32_t Len){
	while (Len > 0)
	    {
	        // Wait until RX buffer is not empty
	        while (SPI_GetFlagStatus(pSPIx, SPI_RXNE_FLAG) == FLAG_RESET);

	        if (pSPIx->CR1 & (1 << SPI_CR1_DFF))
	        {
	            // 16-bit data frame
	            *((uint16_t*)pRXBuffer) = pSPIx->DR;

	            Len--;
	            Len--;

	            (uint16_t*)pRXBuffer++;
	        }
	        else
	        {
	            // 8-bit data frame
	            *pRXBuffer = pSPIx->DR;

	            Len--;
	            pRXBuffer++;
	        }
	    }
}

uint8_t SPI_SendDataIT(SPI_Handle_t *pSPIHandle,uint8_t *pTXBuffer , uint32_t Len){
		uint8_t state = pSPIHandle->TxState;

	    if(state != SPI_BUSY_IN_TX)
	    {
	        // 1. Save the Tx buffer address and length information
	        pSPIHandle->pRxBuffer = pTXBuffer;
	        pSPIHandle->TxLen = Len;

	        // 2. Mark the SPI state as busy in transmission
	        pSPIHandle->TxState = SPI_BUSY_IN_TX;

	        // 3. Enable the TXEIE control bit to get interrupt
	        //    whenever TXE flag is set in SR
	        pSPIHandle->pSPIx->CR2 |= (1 << SPI_CR2_TXEIE);
	    }

	    return state;
}
uint8_t SPI_ReceiveDataIT(SPI_Handle_t *pSPIHandle,uint8_t *pRXBuffer , uint32_t Len){
			uint8_t state = pSPIHandle->RxState;

		    if(state != SPI_BUSY_IN_RX)
		    {
		        // 1. Save the Tx buffer address and length information
		        pSPIHandle->pRxBuffer= pRXBuffer;
		        pSPIHandle->RxLen = Len;

		        // 2. Mark the SPI state as busy in transmission
			    pSPIHandle->RxState= SPI_BUSY_IN_RX;

		        // 3. Enable the TXEIE control bit to get interrupt
		        //    whenever TXE flag is set in SR
		        pSPIHandle->pSPIx->CR2 |= (1 << SPI_CR2_RXNEIE);
		    }

		    return state;
}

void SPI_IRQConfig(uint8_t IRQNumber, uint8_t EnorDi){
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
void SPI_IRQPriority(uint8_t IRQNumber,uint8_t IRQPriority){
		uint8_t iprx= IRQNumber /4;
		uint8_t iprx_section= IRQNumber %4;
		*(NVIC_PR_BASE_ADDR + (iprx *4)) |= (IRQPriority << ((8 * iprx_section) + 4));

}
void SPI_IRQHandling(SPI_Handle_t *pHandle){

}

