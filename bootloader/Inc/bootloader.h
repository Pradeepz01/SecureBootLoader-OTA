#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include <stdint.h>
#define SLOT1_BASE_ADDRESS     0x08010000U /* Sector 4: 64 KB */
#define SLOT2_BASE_ADDRESS     0x08020000U /* Sector 5: 128 KB */
#define SLOT_TABLE_ADDRESS     0x0800C000U /* Sector 3: 16 KB */

#define APPLICATION_ADDRESS    SLOT1_BASE_ADDRESS

#define SRAM_START             0x20000000U
#define SRAM_END               0x20020000U

#define FLASH_END              0x08080000U

#define SCB_VTOR               (*(volatile uint32_t *)0xE000ED08U)

#define SLOT_MAGIC             0x534C4F54U /* ASCII 'SLOT' */

#define SLOT_STATE_EMPTY       0x00
#define SLOT_STATE_VALID       0x01

#define BL_GET_VERSION         0x51
#define BL_FLASH_ERASE         0x52
#define BL_MEM_WRITE           0x53
#define BL_VERIFY_CRC          0x54
#define BL_JUMP_APP            0x55
#define BL_GET_SLOT_INFO       0x56
#define BL_ROLLBACK            0x57
#define BL_ACTIVATE_SLOT       0x58

#define BL_VERSION_MAJOR       1
#define BL_VERSION_MINOR       1
#define BL_VERSION_PATCH       0

#define BL_ACK                 0xA5
#define BL_NACK                0x7F

#define BL_MAX_PACKET_LENGTH   255

typedef struct __attribute__((packed))
{
    uint32_t magic;           /* 0x534C4F54 */
    uint8_t  active_slot;     /* 1 or 2 */
    uint8_t  prev_slot;       /* 1 or 2 (or 0) */
    uint8_t  slot1_state;     /* 0=EMPTY, 1=VALID */
    uint8_t  slot2_state;     /* 0=EMPTY, 1=VALID */
    uint32_t slot1_version;   /* e.g. 0x010000 for v1.0.0 */
    uint32_t slot2_version;   /* e.g. 0x020000 for v2.0.0 */
    uint32_t update_counter;  /* Monotonic sequence counter */
    uint32_t reserved;
    uint32_t crc32;           /* CRC of preceding bytes */
} Bootloader_SlotTable_t;

void SlotTable_Init(void);
Bootloader_SlotTable_t* SlotTable_Get(void);
uint8_t SlotTable_Save(Bootloader_SlotTable_t *pTable);
uint32_t Bootloader_GetActiveSlotAddress(void);

void Bootloader_JumpToApplication(void);
void Bootloader_ReadCommand(void);

void Bootloader_HandleGetVersion(void);
void Bootloader_HandleFlashErase(void);
void Bootloader_HandleMemWrite(uint8_t packet_length);
void Bootloader_HandleJumpApp(void);
void Bootloader_HandleVerifyCRC(uint8_t packet_length);
void Bootloader_HandleGetSlotInfo(void);
void Bootloader_HandleRollback(void);
void Bootloader_HandleActivateSlot(uint8_t packet_length);

void Bootloader_SendACK(void);
void Bootloader_SendNACK(void);

uint32_t Bootloader_CalculateCRC(uint8_t *pData, uint32_t len);
uint8_t Bootloader_VerifyCRC(uint8_t *pData, uint32_t len, uint32_t crc_host);

#endif /* BOOTLOADER_H_ */
