#include "bootloader.h"
#include "stm32f446re_flash_driver.h"
#include "stm32f446re_usart_driver.h"
#include "stm32f446re_gpio_driver.h"
#include <stdio.h>
typedef void (*ApplicationEntry_t)(void);

static inline void Bootloader_SetMSP(uint32_t stack_address)
{
    __asm volatile ("MSR MSP, %0" : : "r" (stack_address) : );
}

static uint8_t bl_rx_buffer[BL_MAX_PACKET_LENGTH];
static USART_RegDef_t *g_bl_usart = USART2;

void Bootloader_SendACK(void)
{
    uint8_t ack = 0xA5;
    USART_SendData(g_bl_usart, &ack, 1);
}

void Bootloader_SendNACK(void)
{
    uint8_t nack = 0x7F;
    USART_SendData(g_bl_usart, &nack, 1);
}

void Bootloader_HandleGetVersion(void)
{
    uint8_t version[3];

    version[0] = BL_VERSION_MAJOR;
    version[1] = BL_VERSION_MINOR;
    version[2] = BL_VERSION_PATCH;

    Bootloader_SendACK();

    USART_SendData(g_bl_usart, version, 3);
}
void Bootloader_HandleMemWrite(uint8_t packet_length)
{
    uint32_t address;
    uint8_t status = FLASH_WRITE_SUCCESS;

    /* Address is in bytes 1..4 (little-endian) */
    address = (uint32_t)bl_rx_buffer[1] |
              ((uint32_t)bl_rx_buffer[2] << 8) |
              ((uint32_t)bl_rx_buffer[3] << 16) |
              ((uint32_t)bl_rx_buffer[4] << 24);

    /* Data length = packet_length - 1 (cmd) - 4 (addr) - 4 (crc) */
    if (packet_length >= 9)
    {
        uint32_t data_len = packet_length - 9;
        if (data_len > 0)
        {
            status = FLASH_ProgramBuffer(address, &bl_rx_buffer[5], data_len);
        }
    }
    else
    {
        status = FLASH_WRITE_FAIL;
    }

    Bootloader_SendACK();

    USART_SendData(g_bl_usart, &status, 1);
}

static Bootloader_SlotTable_t g_slot_table;
static uint8_t g_slot_table_initialized = 0;

static uint8_t Bootloader_ValidateSlot(uint8_t slot_id)
{
    uint32_t base = (slot_id == 2) ? SLOT2_BASE_ADDRESS : SLOT1_BASE_ADDRESS;
    uint32_t sp = *((volatile uint32_t *)base);
    uint32_t reset = *((volatile uint32_t *)(base + 4U));

    if ((sp < SRAM_START) || (sp > SRAM_END)) return 0;
    if ((reset < base) || (reset >= FLASH_END)) return 0;
    return 1;
}

uint8_t SlotTable_Save(Bootloader_SlotTable_t *pTable)
{
    pTable->crc32 = Bootloader_CalculateCRC((uint8_t *)pTable, sizeof(Bootloader_SlotTable_t) - 4);

    FLASH_Unlock();
    FLASH_EraseSector(3); // Sector 3 (0x0800C000, 16 KB)
    uint8_t status = FLASH_ProgramBuffer(SLOT_TABLE_ADDRESS, (uint8_t *)pTable, sizeof(Bootloader_SlotTable_t));
    FLASH_Lock();

    return status;
}

void SlotTable_Init(void)
{
    Bootloader_SlotTable_t *pFlashTable = (Bootloader_SlotTable_t *)SLOT_TABLE_ADDRESS;
    uint8_t table_valid = 0;

    if (pFlashTable->magic == SLOT_MAGIC)
    {
        uint32_t calc_crc = Bootloader_CalculateCRC((uint8_t *)pFlashTable, sizeof(Bootloader_SlotTable_t) - 4);
        if (calc_crc == pFlashTable->crc32)
        {
            g_slot_table = *pFlashTable;
            table_valid = 1;
        }
    }

    if (!table_valid)
    {
        g_slot_table.magic = SLOT_MAGIC;
        g_slot_table.active_slot = 1;
        g_slot_table.prev_slot = 0;
        g_slot_table.slot1_state = Bootloader_ValidateSlot(1) ? SLOT_STATE_VALID : SLOT_STATE_EMPTY;
        g_slot_table.slot2_state = Bootloader_ValidateSlot(2) ? SLOT_STATE_VALID : SLOT_STATE_EMPTY;
        g_slot_table.slot1_version = g_slot_table.slot1_state ? 0x00010000 : 0; // v1.0.0
        g_slot_table.slot2_version = g_slot_table.slot2_state ? 0x00020000 : 0; // v2.0.0
        g_slot_table.update_counter = 1;
        g_slot_table.reserved = 0;
        SlotTable_Save(&g_slot_table);
    }

    g_slot_table_initialized = 1;
}

Bootloader_SlotTable_t* SlotTable_Get(void)
{
    if (!g_slot_table_initialized)
    {
        SlotTable_Init();
    }
    return &g_slot_table;
}

uint32_t Bootloader_GetActiveSlotAddress(void)
{
    if (!g_slot_table_initialized)
    {
        SlotTable_Init();
    }

    if (g_slot_table.active_slot == 2 && Bootloader_ValidateSlot(2))
    {
        return SLOT2_BASE_ADDRESS;
    }
    if (Bootloader_ValidateSlot(1))
    {
        return SLOT1_BASE_ADDRESS;
    }
    if (Bootloader_ValidateSlot(2))
    {
        return SLOT2_BASE_ADDRESS;
    }
    return 0;
}

void Bootloader_HandleJumpApp(void)
{
    uint32_t app_base = Bootloader_GetActiveSlotAddress();
    uint8_t status = 0x00;

    if (app_base == 0)
    {
        status = 0x01; /* Invalid app in both slots */
        Bootloader_SendACK();
        USART_SendData(g_bl_usart, &status, 1);
        return;
    }

    Bootloader_SendACK();
    USART_SendData(g_bl_usart, &status, 1);

    /* Delay to ensure UART transmission finishes */
    for (volatile uint32_t i = 0; i < 50000; i++);

    Bootloader_JumpToApplication();
}

void Bootloader_HandleVerifyCRC(uint8_t packet_length)
{
    uint32_t crc_host = 0;
    uint32_t data_len = 0;

    if (packet_length >= 5)
    {
        data_len = packet_length - 4;
        crc_host = (uint32_t)bl_rx_buffer[packet_length - 4] |
                   ((uint32_t)bl_rx_buffer[packet_length - 3] << 8) |
                   ((uint32_t)bl_rx_buffer[packet_length - 2] << 16) |
                   ((uint32_t)bl_rx_buffer[packet_length - 1] << 24);
    }

    uint32_t calculated_crc = Bootloader_CalculateCRC(bl_rx_buffer, data_len);

    Bootloader_SendACK();

    uint8_t response[5];
    response[0] = (uint8_t)(calculated_crc & 0xFF);
    response[1] = (uint8_t)((calculated_crc >> 8) & 0xFF);
    response[2] = (uint8_t)((calculated_crc >> 16) & 0xFF);
    response[3] = (uint8_t)((calculated_crc >> 24) & 0xFF);
    response[4] = (calculated_crc == crc_host) ? 0x00 : 0x01;

    USART_SendData(g_bl_usart, response, 5);
}

static uint8_t Bootloader_ReceiveByte(USART_RegDef_t *pUSARTx, uint8_t *pByte, uint32_t timeout_loops)
{
    while (timeout_loops--)
    {
        /* Clear ORE / FE / NF if set */
        if (pUSARTx->SR & ((1U << USART_SR_ORE) | (1U << USART_SR_FE) | (1U << USART_SR_NF)))
        {
            volatile uint32_t dummy = pUSARTx->SR;
            dummy = pUSARTx->DR;
            (void)dummy;
        }

        if (pUSARTx->SR & (1U << USART_SR_RXNE))
        {
            *pByte = (uint8_t)(pUSARTx->DR & 0xFF);
            return 1;
        }
    }
    return 0; /* Timeout */
}

void Bootloader_ReadCommand(void)
{
    uint8_t packet_length;
    uint8_t command;

    /* Wait for incoming packet on either USART2 (ST-LINK) or USART1 (ESP8266) */
    while (1)
    {
        /* Clear any error flags that would block RXNE */
        if (USART1->SR & ((1U << USART_SR_ORE) | (1U << USART_SR_FE) | (1U << USART_SR_NF)))
        {
            volatile uint32_t dummy = USART1->SR;
            dummy = USART1->DR;
            (void)dummy;
        }
        if (USART2->SR & ((1U << USART_SR_ORE) | (1U << USART_SR_FE) | (1U << USART_SR_NF)))
        {
            volatile uint32_t dummy = USART2->SR;
            dummy = USART2->DR;
            (void)dummy;
        }

        if (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == FLAG_SET)
        {
            g_bl_usart = USART2;
            break;
        }
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == FLAG_SET)
        {
            g_bl_usart = USART1;
            break;
        }
    }

    /* Receive packet length with timeout */
    if (!Bootloader_ReceiveByte(g_bl_usart, &packet_length, 1000000))
    {
        Bootloader_SendNACK();
        return;
    }

    /* Receive remaining packet bytes with timeout */
    for (uint32_t i = 0; i < packet_length; i++)
    {
        if (!Bootloader_ReceiveByte(g_bl_usart, &bl_rx_buffer[i], 1000000))
        {
            Bootloader_SendNACK();
            return;
        }
    }

    /* A valid packet must contain at least 1 byte command + 4 bytes CRC */
    if (packet_length < 5)
    {
        Bootloader_SendNACK();
        return;
    }

    command = bl_rx_buffer[0];

    /* Extract host CRC from the last 4 bytes (little-endian) */
    uint32_t crc_host = (uint32_t)bl_rx_buffer[packet_length - 4] |
                        ((uint32_t)bl_rx_buffer[packet_length - 3] << 8) |
                        ((uint32_t)bl_rx_buffer[packet_length - 2] << 16) |
                        ((uint32_t)bl_rx_buffer[packet_length - 1] << 24);

    /* Dedicated CRC test/inspection command */
    if (command == BL_VERIFY_CRC)
    {
        Bootloader_HandleVerifyCRC(packet_length);
        return;
    }

    /* Verify packet CRC before processing command */
    if (!Bootloader_VerifyCRC(bl_rx_buffer, packet_length - 4, crc_host))
    {
        Bootloader_SendNACK();
        return;
    }

    switch(command)
    {
        case BL_GET_VERSION:
            Bootloader_HandleGetVersion();
            break;

        case BL_FLASH_ERASE:
            Bootloader_HandleFlashErase();
            break;

        case BL_MEM_WRITE:
            Bootloader_HandleMemWrite(packet_length);
            break;

        case BL_JUMP_APP:
            Bootloader_HandleJumpApp();
            break;

        case BL_GET_SLOT_INFO:
            Bootloader_HandleGetSlotInfo();
            break;

        case BL_ROLLBACK:
            Bootloader_HandleRollback();
            break;

        case BL_ACTIVATE_SLOT:
            Bootloader_HandleActivateSlot(packet_length);
            break;

        case BL_SET_BAUD:
            Bootloader_HandleSetBaud(packet_length);
            break;

        case BL_BENCHMARK_HW:
            Bootloader_HandleBenchmarkHW(packet_length);
            break;

        default:
            Bootloader_SendNACK();
            break;
    }
}

void Bootloader_HandleGetSlotInfo(void)
{
    Bootloader_SlotTable_t *table = SlotTable_Get();

    uint8_t s1_valid = Bootloader_ValidateSlot(1);
    uint8_t s2_valid = Bootloader_ValidateSlot(2);
    table->slot1_state = s1_valid ? SLOT_STATE_VALID : SLOT_STATE_EMPTY;
    table->slot2_state = s2_valid ? SLOT_STATE_VALID : SLOT_STATE_EMPTY;

    uint8_t oldest_slot = 2;
    if (!s1_valid) oldest_slot = 1;
    else if (!s2_valid) oldest_slot = 2;
    else if (table->slot1_version <= table->slot2_version && table->active_slot != 1) oldest_slot = 1;
    else if (table->slot2_version < table->slot1_version && table->active_slot != 2) oldest_slot = 2;
    else oldest_slot = (table->active_slot == 1) ? 2 : 1;

    uint8_t rollback_possible = 0;
    if (table->prev_slot == 1 && s1_valid) rollback_possible = 1;
    else if (table->prev_slot == 2 && s2_valid) rollback_possible = 1;
    else if (table->active_slot == 1 && s2_valid) rollback_possible = 1;
    else if (table->active_slot == 2 && s1_valid) rollback_possible = 1;

    Bootloader_SendACK();

    uint8_t resp[18];
    resp[0] = table->active_slot;
    resp[1] = table->prev_slot;
    resp[2] = table->slot1_state;
    resp[3] = table->slot2_state;

    resp[4] = (uint8_t)(table->slot1_version & 0xFF);
    resp[5] = (uint8_t)((table->slot1_version >> 8) & 0xFF);
    resp[6] = (uint8_t)((table->slot1_version >> 16) & 0xFF);
    resp[7] = (uint8_t)((table->slot1_version >> 24) & 0xFF);

    resp[8] = (uint8_t)(table->slot2_version & 0xFF);
    resp[9] = (uint8_t)((table->slot2_version >> 8) & 0xFF);
    resp[10] = (uint8_t)((table->slot2_version >> 16) & 0xFF);
    resp[11] = (uint8_t)((table->slot2_version >> 24) & 0xFF);

    resp[12] = (uint8_t)(table->update_counter & 0xFF);
    resp[13] = (uint8_t)((table->update_counter >> 8) & 0xFF);
    resp[14] = (uint8_t)((table->update_counter >> 16) & 0xFF);
    resp[15] = (uint8_t)((table->update_counter >> 24) & 0xFF);

    resp[16] = oldest_slot;
    resp[17] = rollback_possible;

    USART_SendData(g_bl_usart, resp, 18);
}

void Bootloader_HandleActivateSlot(uint8_t packet_length)
{
    uint8_t status = 0x00;
    uint8_t target_slot = bl_rx_buffer[1];
    uint32_t version = (uint32_t)bl_rx_buffer[2] |
                       ((uint32_t)bl_rx_buffer[3] << 8) |
                       ((uint32_t)bl_rx_buffer[4] << 16) |
                       ((uint32_t)bl_rx_buffer[5] << 24);

    if (target_slot != 1 && target_slot != 2)
    {
        status = 0x01;
    }
    else if (!Bootloader_ValidateSlot(target_slot))
    {
        status = 0x02;
    }
    else
    {
        Bootloader_SlotTable_t *table = SlotTable_Get();
        table->prev_slot = table->active_slot;
        table->active_slot = target_slot;
        table->update_counter++;

        if (target_slot == 1)
        {
            table->slot1_state = SLOT_STATE_VALID;
            table->slot1_version = version;
        }
        else
        {
            table->slot2_state = SLOT_STATE_VALID;
            table->slot2_version = version;
        }

        SlotTable_Save(table);
    }

    Bootloader_SendACK();
    USART_SendData(g_bl_usart, &status, 1);
}

void Bootloader_HandleRollback(void)
{
    Bootloader_SlotTable_t *table = SlotTable_Get();
    uint8_t target_slot = 0;
    uint8_t status = 0x00;

    if (table->prev_slot == 1 && Bootloader_ValidateSlot(1))
    {
        target_slot = 1;
    }
    else if (table->prev_slot == 2 && Bootloader_ValidateSlot(2))
    {
        target_slot = 2;
    }
    else if (table->active_slot == 1 && Bootloader_ValidateSlot(2))
    {
        target_slot = 2;
    }
    else if (table->active_slot == 2 && Bootloader_ValidateSlot(1))
    {
        target_slot = 1;
    }

    if (target_slot == 0)
    {
        status = 0x01; /* Rollback not possible */
        Bootloader_SendACK();
        USART_SendData(g_bl_usart, &status, 1);
        return;
    }

    table->prev_slot = table->active_slot;
    table->active_slot = target_slot;
    table->update_counter++;
    SlotTable_Save(table);

    Bootloader_SendACK();
    uint8_t resp[2];
    resp[0] = 0x00; /* Success */
    resp[1] = target_slot;
    USART_SendData(g_bl_usart, resp, 2);

    for (volatile uint32_t i = 0; i < 50000; i++);
    Bootloader_JumpToApplication();
}

uint32_t Bootloader_CalculateCRC(uint8_t *pData, uint32_t len)
{
    CRC->CR |= (1 << 0);     // Reset CRC calculator

    for(uint32_t i = 0; i < len; i++)
    {
        CRC->DR = pData[i];
    }

    return CRC->DR;
}

uint8_t Bootloader_VerifyCRC(uint8_t *pData, uint32_t len, uint32_t crc_host)
{
    uint32_t crc = Bootloader_CalculateCRC(pData, len);

    if(crc == crc_host)
        return 1;

    return 0;
}

void Bootloader_HandleFlashErase(void)
{
    uint8_t sector;
    uint8_t num_of_sectors;
    uint8_t status = FLASH_ERASE_SUCCESS;

    /* Packet Layout:
       bl_rx_buffer[0] = Command
       bl_rx_buffer[1] = Sector
       bl_rx_buffer[2] = Number of sectors
    */

    sector = bl_rx_buffer[1];
    num_of_sectors = bl_rx_buffer[2];

    FLASH_Unlock();

    for(uint8_t i = 0; i < num_of_sectors; i++)
    {
        FLASH_EraseSector(sector + i);
    }

    FLASH_Lock();

    Bootloader_SendACK();

    USART_SendData(g_bl_usart, &status, 1);
}


void Bootloader_JumpToApplication(void)
{
    uint32_t app_base = Bootloader_GetActiveSlotAddress();
    if (app_base == 0)
    {
        return;
    }

    uint32_t app_stack_pointer = *((volatile uint32_t *)app_base);
    uint32_t app_reset_handler = *((volatile uint32_t *)(app_base + 4U));

    /* Turn off LED PA5 and disable USART peripheral clocks before jump */
    GPIO_WriteToOutputPin(GPIOA, GPIO_PIN_NO_5, 0);
    USART2_PCLK_DI();
    USART1_PCLK_DI();

    /* Redirect interrupts to the active application's vector table */
    SCB_VTOR = app_base;

    /* Set MSP and branch to application Reset_Handler */
    __asm volatile (
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r" (app_stack_pointer), "r" (app_reset_handler)
        : "memory"
    );

    /* Should never reach here */
    while (1)
    {
    	 Bootloader_ReadCommand();
    }
}

void Bootloader_HandleSetBaud(uint8_t packet_length)
{
    uint32_t baud;
    /* baud rate in bl_rx_buffer[1..4] */
    baud = (uint32_t)bl_rx_buffer[1] |
           ((uint32_t)bl_rx_buffer[2] << 8) |
           ((uint32_t)bl_rx_buffer[3] << 16) |
           ((uint32_t)bl_rx_buffer[4] << 24);

    Bootloader_SendACK();
    uint8_t status = 0x00;
    USART_SendData(g_bl_usart, &status, 1);

    /* Wait until transmission complete (TC=1) before changing BRR */
    while (USART_GetFlagStatus(g_bl_usart, USART_FLAG_TC) == FLAG_RESET);

    /* Short stabilization delay */
    for (volatile uint32_t i = 0; i < 5000; i++);

    /* Reconfigure baud rate */
    USART_SetBaudRate(g_bl_usart, baud);
}

void Bootloader_HandleBenchmarkHW(uint8_t packet_length)
{
    uint8_t subcmd = bl_rx_buffer[1];

    if (subcmd == 0x01) /* Subcmd 1: Boot Initialization Timing */
    {
        Bootloader_SendACK();
        uint8_t resp[9];
        resp[0] = 0x00; /* status */
        resp[1] = (uint8_t)(g_boot_init_cycles & 0xFF);
        resp[2] = (uint8_t)((g_boot_init_cycles >> 8) & 0xFF);
        resp[3] = (uint8_t)((g_boot_init_cycles >> 16) & 0xFF);
        resp[4] = (uint8_t)((g_boot_init_cycles >> 24) & 0xFF);
        uint32_t clk = 16000000;
        resp[5] = (uint8_t)(clk & 0xFF);
        resp[6] = (uint8_t)((clk >> 8) & 0xFF);
        resp[7] = (uint8_t)((clk >> 16) & 0xFF);
        resp[8] = (uint8_t)((clk >> 24) & 0xFF);
        USART_SendData(g_bl_usart, resp, 9);
    }
    else if (subcmd == 0x02) /* Subcmd 2: Pure Flash Word / Chunk Programming Benchmark */
    {
        uint32_t num_words = bl_rx_buffer[2]; /* 1, 16, 32 words */
        if (num_words == 0 || num_words > 32) num_words = 1;
        uint32_t test_addr = 0x08010000U;
        uint32_t test_data = 0xA5A55A5AU;

        FLASH_Unlock();
        uint32_t t_start = DWT_CYCCNT;
        for (uint32_t w = 0; w < num_words; w++)
        {
            FLASH_ProgramWord(test_addr + (w * 4U), test_data);
        }
        uint32_t t_elapsed = DWT_CYCCNT - t_start;
        FLASH_Lock();

        Bootloader_SendACK();
        uint8_t resp[9];
        resp[0] = 0x00;
        resp[1] = (uint8_t)(t_elapsed & 0xFF);
        resp[2] = (uint8_t)((t_elapsed >> 8) & 0xFF);
        resp[3] = (uint8_t)((t_elapsed >> 16) & 0xFF);
        resp[4] = (uint8_t)((t_elapsed >> 24) & 0xFF);
        uint32_t bytes_prog = num_words * 4U;
        resp[5] = (uint8_t)(bytes_prog & 0xFF);
        resp[6] = (uint8_t)((bytes_prog >> 8) & 0xFF);
        resp[7] = (uint8_t)((bytes_prog >> 16) & 0xFF);
        resp[8] = (uint8_t)((bytes_prog >> 24) & 0xFF);
        USART_SendData(g_bl_usart, resp, 9);
    }
    else if (subcmd == 0x03) /* Subcmd 3: Pure Hardware CRC-32 Execution Benchmark */
    {
        uint32_t req_size = (uint32_t)bl_rx_buffer[2] | ((uint32_t)bl_rx_buffer[3] << 8);
        if (req_size == 0 || req_size > 4096) req_size = 64;

        static uint8_t crc_test_buf[4096];
        for (uint32_t i = 0; i < req_size; i++)
        {
            crc_test_buf[i] = (uint8_t)(i ^ 0x5A);
        }

        uint32_t t_start = DWT_CYCCNT;
        uint32_t crc_val = Bootloader_CalculateCRC(crc_test_buf, req_size);
        uint32_t t_elapsed = DWT_CYCCNT - t_start;

        Bootloader_SendACK();
        uint8_t resp[13];
        resp[0] = 0x00;
        resp[1] = (uint8_t)(crc_val & 0xFF);
        resp[2] = (uint8_t)((crc_val >> 8) & 0xFF);
        resp[3] = (uint8_t)((crc_val >> 16) & 0xFF);
        resp[4] = (uint8_t)((crc_val >> 24) & 0xFF);
        resp[5] = (uint8_t)(t_elapsed & 0xFF);
        resp[6] = (uint8_t)((t_elapsed >> 8) & 0xFF);
        resp[7] = (uint8_t)((t_elapsed >> 16) & 0xFF);
        resp[8] = (uint8_t)((t_elapsed >> 24) & 0xFF);
        resp[9] = (uint8_t)(req_size & 0xFF);
        resp[10] = (uint8_t)((req_size >> 8) & 0xFF);
        resp[11] = (uint8_t)((req_size >> 16) & 0xFF);
        resp[12] = (uint8_t)((req_size >> 24) & 0xFF);
        USART_SendData(g_bl_usart, resp, 13);
    }
    else if (subcmd == 0x04) /* Subcmd 4: Pure Flash Erase Benchmark */
    {
        uint8_t sec = bl_rx_buffer[2];
        FLASH_Unlock();
        uint32_t t_start = DWT_CYCCNT;
        FLASH_EraseSector(sec);
        uint32_t t_elapsed = DWT_CYCCNT - t_start;
        FLASH_Lock();

        Bootloader_SendACK();
        uint8_t resp[5];
        resp[0] = 0x00;
        resp[1] = (uint8_t)(t_elapsed & 0xFF);
        resp[2] = (uint8_t)((t_elapsed >> 8) & 0xFF);
        resp[3] = (uint8_t)((t_elapsed >> 16) & 0xFF);
        resp[4] = (uint8_t)((t_elapsed >> 24) & 0xFF);
        USART_SendData(g_bl_usart, resp, 5);
    }
    else
    {
        Bootloader_SendNACK();
    }
}
