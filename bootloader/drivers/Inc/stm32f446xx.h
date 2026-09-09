/*
 * stm32f446xx.h
 *
 *  Created on: Jul 5, 2026
 *      Author: HP
 */

#ifndef INC_STM32F446XX_H_
#define INC_STM32F446XX_H_

#include <stdint.h>
#include <stddef.h>

#define __vo volatile
#define __weak				__attribute__((weak))


#define ENABLE				1
#define DISABLE				0
#define SET					ENABLE
#define RESET				DISABLE
#define GPIO_PIN_SET		SET
#define GPIO_PIN_RESET		RESET
#define FLAG_RESET			RESET
#define FLAG_SET			SET


/*
 * ARM Cortex Mx NVIC ISERx Register Addresses
 */
#define NVIC_ISER0			((__vo uint32_t*) 0xE000E100)
#define NVIC_ISER1			((__vo uint32_t*) 0xE000E104)
#define NVIC_ISER2			((__vo uint32_t*) 0xE000E108)
#define NVIC_ISER3			((__vo uint32_t*) 0xE000E10C)

/*
 * ARM Cortex Mx NVIC ICERx Register Addresses
 */
#define NVIC_ICER0			((__vo uint32_t*) 0xE000E180)
#define NVIC_ICER1			((__vo uint32_t*) 0xE000E184)
#define NVIC_ICER2			((__vo uint32_t*) 0xE000E188)
#define NVIC_ICER3			((__vo uint32_t*) 0xE000E18C)


#define NVIC_PR_BASE_ADDR   ((__vo uint32_t*) 0xE000E400)









#define RCC_BASEADDR 		0x40023800

/*  MEMORY ADDRESSES                              */
#define FLASH_BASEADDR		0x08000000U
#define SRAM1_BASEADDR		0x20000000U
#define SRAM 				SRAM1_BASEADDR
#define SRAM2_BASEADDR  	0x2001C000U
#define ROM 				0x1FFF0000U

typedef struct
{
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t OPTCR;

}FLASH_RegDef_t;

#define FLASH_REG_BASEADDR  0x40023C00U
#define FLASH_REGS			((FLASH_RegDef_t *)FLASH_REG_BASEADDR)
/* FLASH_SR */

#define FLASH_SR_EOP          0
#define FLASH_SR_OPERR        1
#define FLASH_SR_WRPERR       4
#define FLASH_SR_PGAERR       5
#define FLASH_SR_PGPERR       6
#define FLASH_SR_PGSERR       7
#define FLASH_SR_BSY          16

/* FLASH_CR */

#define FLASH_CR_PG           0
#define FLASH_CR_SER          1
#define FLASH_CR_MER          2
#define FLASH_CR_SNB          3
#define FLASH_CR_PSIZE        8
#define FLASH_CR_STRT         16
#define FLASH_CR_LOCK         31
/* Flash Unlock Keys */

#define FLASH_KEY1    0x45670123U
#define FLASH_KEY2    0xCDEF89ABU
/* PERIPHERAL ADDRESSES                                */

#define PERIPH_BASEADDR		0x40000000U
#define APB1PERIPH_BASEADDR	0x40000000U
#define APB2PERIPH_BASEADDR	0x40010000U
#define AHB1PERIPH_BASEADDR	0x40020000U
#define AHB2PERIPH_BASEADDR	0x50000000U


/* GPIO BASE ADDRESSES                                */
#define GPIOA_BASEADDR 		0x40020000U
#define GPIOB_BASEADDR 		0x40020400U
#define GPIOC_BASEADDR 		0x40020800U
#define GPIOD_BASEADDR 		0x40020C00U
#define GPIOE_BASEADDR 		0x40021000U
#define GPIOF_BASEADDR 		0x40021400U
#define GPIOG_BASEADDR 		0x40021800U
#define GPIOH_BASEADDR 		0x40021C00U


/* APB1 PERIPHERAL BASE ADDRESSES                               */


#define SPI2_BASEADDR		(APB1PERIPH_BASEADDR + 0x3800)
#define SPI3_BASEADDR		(APB1PERIPH_BASEADDR + 0x3C00)

#define USART2_BASEADDR		(APB1PERIPH_BASEADDR + 0x4400)
#define USART3_BASEADDR		(APB1PERIPH_BASEADDR + 0x4800)

#define UART4_BASEADDR		(APB1PERIPH_BASEADDR + 0x4C00)
#define UART5_BASEADDR		(APB1PERIPH_BASEADDR + 0x5000)

#define I2C1_BASEADDR		(APB1PERIPH_BASEADDR + 0x5400)
#define I2C2_BASEADDR		(APB1PERIPH_BASEADDR + 0x5800)
#define I2C3_BASEADDR		(APB1PERIPH_BASEADDR + 0x5C00)



/* APB2 PERIPHERAL BASE ADDRESSES                                */
#define USART1_BASEADDR		(APB2PERIPH_BASEADDR + 0x1000)
#define USART6_BASEADDR		(APB2PERIPH_BASEADDR + 0x1400)

#define SPI1_BASEADDR		(APB2PERIPH_BASEADDR + 0x3000)
#define SPI4_BASEADDR		(APB2PERIPH_BASEADDR + 0x3400)

#define SYSCFG_BASEADDR		(APB2PERIPH_BASEADDR + 0x3800)
#define EXTI_BASEADDR		(APB2PERIPH_BASEADDR + 0x3C00)


/*GPIO REGISTERS*/
typedef struct {
	volatile uint32_t MODER;
	volatile uint32_t OTYPER;
	volatile uint32_t OSPEEDR;
	volatile uint32_t PUPDR;
	volatile uint32_t IDR ;
	volatile uint32_t ODR ;
	volatile uint32_t BSRR;
	volatile uint32_t LCKR;
	volatile uint32_t AFR[2];
}GPIO_RegDef_t;

#define GPIOA				((GPIO_RegDef_t*)GPIOA_BASEADDR)
#define GPIOB				((GPIO_RegDef_t*)GPIOB_BASEADDR)
#define GPIOC				((GPIO_RegDef_t*)GPIOC_BASEADDR)
#define GPIOD				((GPIO_RegDef_t*)GPIOD_BASEADDR)
#define GPIOE				((GPIO_RegDef_t*)GPIOE_BASEADDR)
#define GPIOF				((GPIO_RegDef_t*)GPIOF_BASEADDR)
#define GPIOG				((GPIO_RegDef_t*)GPIOG_BASEADDR)
#define GPIOH				((GPIO_RegDef_t*)GPIOH_BASEADDR)


typedef struct
{
    __vo uint32_t CR;
    __vo uint32_t PLLCFGR;
    __vo uint32_t CFGR;
    __vo uint32_t CIR;
    __vo uint32_t AHB1RSTR;
    __vo uint32_t AHB2RSTR;
    __vo uint32_t AHB3RSTR;
    uint32_t RESERVED0;
    __vo uint32_t APB1RSTR;
    __vo uint32_t APB2RSTR;
    uint32_t RESERVED1[2];
    __vo uint32_t AHB1ENR;
    __vo uint32_t AHB2ENR;
    __vo uint32_t AHB3ENR;
    uint32_t RESERVED2;
    __vo uint32_t APB1ENR;
    __vo uint32_t APB2ENR;
    uint32_t RESERVED3[2];
    __vo uint32_t BDCR;
    __vo uint32_t CSR;
    uint32_t RESERVED4[2];
    __vo uint32_t SSCGR;
    __vo uint32_t PLLI2SCFGR;
    __vo uint32_t PLLSAICFGR;
    __vo uint32_t DCKCFGR;
    __vo uint32_t CKGATENR;
    __vo uint32_t DCKCFGR2;

} RCC_RegDef_t;


#define RCC				((RCC_RegDef_t*)RCC_BASEADDR)

uint32_t RCC_GetPCLK1Value(void);
uint32_t RCC_GetPCLK2Value(void);


/*SPI Registers*/
typedef struct
{
	__vo uint32_t CR1;
	__vo uint32_t CR2;
	__vo uint32_t SR;
	__vo uint32_t DR;
	__vo uint32_t CRCPR;
	__vo uint32_t RXCRCR;
	__vo uint32_t TXCRCR;
	__vo uint32_t I2SCFGR;
	__vo uint32_t I2SPR;
}SPI_RegDef_t;


#define SPI1				((SPI_RegDef_t*) SPI1_BASEADDR)
#define SPI2				((SPI_RegDef_t*) SPI2_BASEADDR)
#define SPI3				((SPI_RegDef_t*) SPI3_BASEADDR)
#define SPI4				((SPI_RegDef_t*) SPI4_BASEADDR)


#define CRC_BASEADDR    	0x40023000UL
#define CRC_PCLK_EN()     (RCC->AHB1ENR |= (1 << 12))
#define CRC_PCLK_DI()     (RCC->AHB1ENR &= ~(1 << 12))

typedef struct
{
    volatile uint32_t DR;      /* Data Register            Offset: 0x00 */
    volatile uint8_t  IDR;     /* Independent Data Register Offset: 0x04 */
    uint8_t RESERVED0[3];
    volatile uint32_t CR;      /* Control Register         Offset: 0x08 */
} CRC_RegDef_t;

#define CRC ((CRC_RegDef_t *)CRC_BASEADDR)



#define GPIOA_PCLK_EN()  (RCC->AHB1ENR |=(1<<0))
#define GPIOB_PCLK_EN()  (RCC->AHB1ENR |=(1<<1))
#define GPIOC_PCLK_EN()  (RCC->AHB1ENR |=(1<<2))
#define GPIOD_PCLK_EN()  (RCC->AHB1ENR |=(1<<3))
#define GPIOE_PCLK_EN()  (RCC->AHB1ENR |=(1<<4))
#define GPIOF_PCLK_EN()  (RCC->AHB1ENR |=(1<<5))
#define GPIOG_PCLK_EN()  (RCC->AHB1ENR |=(1<<6))
#define GPIOH_PCLK_EN()  (RCC->AHB1ENR |=(1<<7))


#define GPIOA_PCLK_DI()  (RCC->AHB1ENR &= ~(1U << 0))
#define GPIOB_PCLK_DI()  (RCC->AHB1ENR &= ~(1U << 1))
#define GPIOC_PCLK_DI()  (RCC->AHB1ENR &= ~(1U << 2))
#define GPIOD_PCLK_DI()  (RCC->AHB1ENR &= ~(1U << 3))
#define GPIOE_PCLK_DI()  (RCC->AHB1ENR &= ~(1U << 4))
#define GPIOF_PCLK_DI()  (RCC->AHB1ENR &= ~(1U << 5))
#define GPIOG_PCLK_DI()  (RCC->AHB1ENR &= ~(1U << 6))
#define GPIOH_PCLK_DI()  (RCC->AHB1ENR &= ~(1U << 7))


#define GPIOA_REG_RESET()	do{( RCC -> AHB1RSTR |= ( 1<<0 )); ( RCC -> AHB1RSTR &= ~( 1<<0 )); }while(0)
#define GPIOB_REG_RESET()	do{( RCC -> AHB1RSTR |= ( 1<<1 )); ( RCC -> AHB1RSTR &= ~( 1<<1 )); }while(0)
#define GPIOC_REG_RESET()	do{( RCC -> AHB1RSTR |= ( 1<<2 )); ( RCC -> AHB1RSTR &= ~( 1<<2 )); }while(0)
#define GPIOD_REG_RESET()	do{( RCC -> AHB1RSTR |= ( 1<<3 )); ( RCC -> AHB1RSTR &= ~( 1<<3 )); }while(0)
#define GPIOE_REG_RESET()	do{( RCC -> AHB1RSTR |= ( 1<<4 )); ( RCC -> AHB1RSTR &= ~( 1<<4 )); }while(0)
#define GPIOF_REG_RESET()	do{( RCC -> AHB1RSTR |= ( 1<<5 )); ( RCC -> AHB1RSTR &= ~( 1<<5 )); }while(0)
#define GPIOG_REG_RESET()	do{( RCC -> AHB1RSTR |= ( 1<<6 )); ( RCC -> AHB1RSTR &= ~( 1<<6 )); }while(0)
#define GPIOH_REG_RESET()	do{( RCC -> AHB1RSTR |= ( 1<<7 )); ( RCC -> AHB1RSTR &= ~( 1<<7 )); }while(0)


#define GPIO_BASEADDR_TO_CODE(x) 	(	(x == GPIOA) ? 0 : \
										(x == GPIOB) ? 1 : \
										(x == GPIOC) ? 2 : \
										(x == GPIOD) ? 3 : \
										(x == GPIOE) ? 4 : \
										(x == GPIOF) ? 5 : \
										(x == GPIOG) ? 6 : \
										(x == GPIOH) ? 7 : 0	)




typedef struct
{
    volatile uint32_t SR;      /* Offset: 0x00 */
    volatile uint32_t DR;      /* Offset: 0x04 */
    volatile uint32_t BRR;     /* Offset: 0x08 */
    volatile uint32_t CR1;     /* Offset: 0x0C */
    volatile uint32_t CR2;     /* Offset: 0x10 */
    volatile uint32_t CR3;     /* Offset: 0x14 */
    volatile uint32_t GTPR;    /* Offset: 0x18 */
} USART_RegDef_t;


#define USART1 ((USART_RegDef_t *)USART1_BASEADDR)
#define USART2 ((USART_RegDef_t *)USART2_BASEADDR)
#define USART6 ((USART_RegDef_t *)USART6_BASEADDR)


#define USART1_REG_RESET() \
do{ \
    RCC->APB2RSTR |= (1 << 4); \
    RCC->APB2RSTR &= ~(1 << 4); \
}while(0)

#define USART2_REG_RESET() \
do{ \
    RCC->APB1RSTR |= (1 << 17); \
    RCC->APB1RSTR &= ~(1 << 17); \
}while(0)

#define USART6_REG_RESET() \
do{ \
    RCC->APB2RSTR |= (1 << 5); \
    RCC->APB2RSTR &= ~(1 << 5); \
}while(0)

#define SPI2_PCLK_EN() 	 (RCC-> APB1ENR |=(1<<14))
#define SPI3_PCLK_EN() 	 (RCC-> APB1ENR |=(1<<15))
#define USART2_PCLK_EN() (RCC-> APB1ENR |=(1<<17))
#define USART3_PCLK_EN() (RCC-> APB1ENR |=(1<<18))
#define UART4_PCLK_EN()  (RCC-> APB1ENR |=(1<<19))
#define UART5_PCLK_EN()  (RCC-> APB1ENR |=(1<<20))
#define I2C1_PCLK_EN() 	 (RCC-> APB1ENR |=(1<<21))
#define I2C2_PCLK_EN() 	 (RCC-> APB1ENR |=(1<<22))
#define I2C3_PCLK_EN() 	 (RCC-> APB1ENR |=(1<<23))

#define SPI2_PCLK_DI()		( RCC -> APB1ENR &= ~( 1<<14 ))
#define SPI3_PCLK_DI()		( RCC -> APB1ENR &= ~( 1<<15 ))
#define USART2_PCLK_DI()	( RCC -> APB1ENR &= ~( 1<<17 ))
#define USART3_PCLK_DI()	( RCC -> APB1ENR &= ~( 1<<18 ))
#define UART4_PCLK_DI()		( RCC -> APB1ENR &= ~( 1<<19 ))
#define UART5_PCLK_DI()		( RCC -> APB1ENR &= ~( 1<<20 ))
#define I2C1_PCLK_DI()		( RCC -> APB1ENR &= ~( 1<<21 ))
#define I2C2_PCLK_DI()		( RCC -> APB1ENR &= ~( 1<<22 ))
#define I2C3_PCLK_DI()		( RCC -> APB1ENR &= ~( 1<<23 ))



#define SPI1_PCLK_EN() 	 (RCC-> APB2ENR |=(1<<12))
#define SPI4_PCLK_EN() 	 (RCC-> APB2ENR |=(1<<13))
#define SYSCFG_PCLK_EN() ( RCC -> APB2ENR |= ( 1<<14 ))
#define USART1_PCLK_EN() (RCC-> APB2ENR |=(1<<4))
#define USART6_PCLK_EN() (RCC-> APB2ENR |=(1<<5))



#define SPI1_PCLK_DI()		( RCC -> APB2ENR &= ~( 1<<12 ))
#define SPI4_PCLK_DI()		( RCC -> APB2ENR &= ~( 1<<13 ))

#define SYSCFG_PCLK_DI()	( RCC -> APB2ENR &= ~( 1<<14 ))
#define USART1_PCLK_DI()	( RCC -> APB2ENR &= ~( 1<<4 ))
#define USART6_PCLK_DI()	( RCC -> APB2ENR &= ~( 1<<5 ))


#define SPI1_REG_RESET()	do{( RCC -> APB2RSTR |= ( 1<<12 )); ( RCC -> APB2RSTR &= ~( 1<<12 )); }while(0)
#define SPI2_REG_RESET()	do{( RCC -> APB1RSTR |= ( 1<<14 )); ( RCC -> APB1RSTR &= ~( 1<<14 )); }while(0)
#define SPI3_REG_RESET()	do{( RCC -> APB1RSTR |= ( 1<<15 )); ( RCC -> APB1RSTR &= ~( 1<<15 )); }while(0)
#define SPI4_REG_RESET()	do{( RCC -> APB2RSTR |= ( 1<<13 )); ( RCC -> APB2RSTR &= ~( 1<<13 )); }while(0)


typedef struct
{
	__vo uint32_t IMR;
	__vo uint32_t EMR;
	__vo uint32_t RTSR;
	__vo uint32_t FTSR;
	__vo uint32_t SWIER;
	__vo uint32_t PR;
}EXTI_RegDef_t;

#define EXTI ((EXTI_RegDef_t*) EXTI_BASEADDR)

/*
 * SYSCFG Registers - Reference Manual: Page Number. 301/1347
 */
typedef struct
{
	__vo uint32_t MEMRMP;
	__vo uint32_t PMC;
	__vo uint32_t EXTICR[4];
	__vo uint32_t RESERVED1[2];
	__vo uint32_t CMPCR;
	__vo uint32_t RESERVED2[2];
	__vo uint32_t CFGR;
}SYSCFG_Reg_t;

#define SYSCFG ((SYSCFG_Reg_t*) SYSCFG_BASEADDR)



#define IRQ_NO_EXTI0		6
#define IRQ_NO_EXTI1		7
#define IRQ_NO_EXTI2		8
#define IRQ_NO_EXTI3		9
#define IRQ_NO_EXTI4		10
#define IRQ_NO_EXTI9_5		23
#define IRQ_NO_EXTI15_10	40
#define IRQ_NO_SPI1			35
#define IRQ_NO_SPI2         36
#define IRQ_NO_SPI3         51

/*
 * macros for all the possible priority levels
 */
#define NVIC_IRQ_PRI0    	0
#define NVIC_IRQ_PRI15    	15


/*
 * BIT Position Macros of SPI Peripharels
 */

/*SPI_CR1*/
#define SPI_CR1_CPHA		0
#define SPI_CR1_CPOL		1
#define SPI_CR1_MSTR		2
#define SPI_CR1_BR			3
#define SPI_CR1_SPE			6
#define SPI_CR1_LSBFIRST	7
#define SPI_CR1_SSI			8
#define SPI_CR1_SSM			9
#define SPI_CR1_RXONLY		10
#define SPI_CR1_DFF			11
#define SPI_CR1_CRCNEXT		12
#define SPI_CR1_CRCEN		13
#define SPI_CR1_BIDIOE		14
#define SPI_CR1_BIDIMODE	15
/*SPI_CR2*/
#define SPI_CR2_RXDMAEN		0
#define SPI_CR2_TXDMAEN		1
#define SPI_CR2_SSOE		2
#define SPI_CR2_FRF			4
#define SPI_CR2_ERRIE		5
#define SPI_CR2_RXNEIE		6
#define SPI_CR2_TXEIE		7
/*SPI_SR*/
#define SPI_SR_RXNE			0
#define SPI_SR_TXE			1
#define SPI_SR_CHSIDE		2
#define SPI_SR_UDR			3
#define SPI_SR_CRCERR		4
#define SPI_SR_MODF			5
#define SPI_SR_OVR			6
#define SPI_SR_BSY			7
#define SPI_SR_FRE			8


/* USART_CR1 Bit Positions */
#define USART_CR1_SBK        0
#define USART_CR1_RWU        1
#define USART_CR1_RE         2
#define USART_CR1_TE         3
#define USART_CR1_IDLEIE     4
#define USART_CR1_RXNEIE     5
#define USART_CR1_TCIE       6
#define USART_CR1_TXEIE      7
#define USART_CR1_PEIE       8
#define USART_CR1_PS         9
#define USART_CR1_PCE        10
#define USART_CR1_WAKE       11
#define USART_CR1_M          12
#define USART_CR1_UE         13
#define USART_CR1_OVER8      15

/* USART_CR2 Bit Positions */
#define USART_CR2_STOP       12


#endif /* INC_STM32F446XX_H_ */
