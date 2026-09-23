# SecureBootLoader-OTA

Bare-metal secure bootloader + OTA firmware update system for the **STM32F446RE**, built from scratch in register-level Embedded C — no HAL.

> **Status:** 🚀 **Phases 1–5 Implemented & Hardware Tested on STM32F446RE**  
> Features working: Register-level Drivers • Packet Protocol • Hardware CRC-32 Verification • Dual-Slot (A/B) Flash Management • Sector 3 Metadata Table • Automatic Oldest-Slot Replacement • Instant Rollback • Host Python Tool.

---

## 📄 Overview

This project implements an embedded secure bootloader and dual-slot update system for the **STM32F446RE** (ARM Cortex-M4) entirely at the register level (no STM32 HAL or LL libraries).

The system partitions internal flash memory into independent execution slots (**Slot 1** and **Slot 2**) and manages runtime metadata in a dedicated flash sector. New firmware images are streamed over UART using a framed packet protocol with **hardware CRC-32** integrity checking. The bootloader automatically identifies the oldest slot, flashes the new image, updates the metadata table, and boots into the new version. If an update causes issues, a single rollback command instantly restores the previous working version.

---

## 🗺️ Flash Memory Layout (STM32F446RE - 512 KB)

| Sector | Address Range | Size | Description |
|---|---|---|---|
| **Sector 0** | `0x0800 0000 - 0x0800 3FFF` | 16 KB | Bootloader Code & Vector Table |
| **Sector 1** | `0x0800 4000 - 0x0800 7FFF` | 16 KB | Bootloader Code |
| **Sector 2** | `0x0800 8000 - 0x0800 BFFF` | 16 KB | Bootloader Code |
| **Sector 3** | `0x0800 C000 - 0x0800 FFFF` | 16 KB | **Metadata Table** (Slot info, active flag, versions, CRCs) |
| **Sector 4** | `0x0801 0000 - 0x0801 FFFF` | 64 KB | **Application Slot 1** (Firmware A) |
| **Sector 5** | `0x0802 0000 - 0x0803 FFFF` | 128 KB | **Application Slot 2** (Firmware B) |
| **Sector 6** | `0x0804 0000 - 0x0805 FFFF` | 128 KB | Reserved / User Storage |
| **Sector 7** | `0x0806 0000 - 0x0807 FFFF` | 128 KB | Reserved / User Storage |

---

## 📦 Packet Protocol & CRC-32 Verification

Every frame sent between the host PC and the bootloader is framed and protected by STM32's hardware CRC engine:

```
+-------------+--------------+-----------------+------------------+-----------------+
| START BYTE  | COMMAND CODE | LENGTH (Bytes)  | DATA PAYLOAD     | CRC-32 (4 Bytes)|
| 1 Byte (A5) | 1 Byte       | 1 Byte (0-255)  | N Bytes          | Big-Endian      |
+-------------+--------------+-----------------+------------------+-----------------+
```

- **Start Byte:** `0xA5`
- **CRC-32 Polynomial:** `0x04C11DB7` (Standard Ethernet / STM32 Hardware CRC Engine, init `0xFFFFFFFF`)
- **Verification:** The bootloader computes CRC-32 over `[COMMAND, LENGTH, DATA...]` using STM32's on-chip CRC peripheral. The packet is processed only if computed CRC matches received CRC; otherwise, an immediate `NACK (0x7F)` is returned.

---

## 📡 Supported Bootloader Commands

| Command | Opcode | Description |
|---|---|---|
| `BL_GET_VER` | `0x51` | Returns bootloader version (e.g., `0x10` for v1.0) |
| `BL_GET_CHIP_ID` | `0x52` | Returns STM32 MCU Chip ID (`0x0446`) |
| `BL_FLASH_ERASE` | `0x53` | Erases specified flash sector (0–7) with sector protection |
| `BL_MEM_WRITE` | `0x54` | Writes up to 128 bytes to flash at designated address |
| `BL_GO_TO_ADDR` | `0x55` | Relocates vector table (`VTOR`) and executes application |
| `BL_GET_SLOT_INFO` | `0x56` | Returns current metadata: active slot, slot 1/2 validity, versions, sizes, and CRCs |
| `BL_ROLLBACK` | `0x57` | Switches active slot to the alternate valid slot, persists to Sector 3, and boots |
| `BL_ACTIVATE_SLOT` | `0x58` | Sets designated slot (1 or 2) as active in metadata table |

---

## 🔄 Dual-Slot (A/B) Update & Rollback Mechanism

1. **Active Boot Check:** On power-up, the bootloader reads Sector 3 (`0x0800C000`). If valid, it checks the active slot's initial stack pointer and reset handler, relocates `SCB->VTOR`, and branches to user code.
2. **Oldest-Slot Replacement:** When `--update` is issued, the host tool queries slot metadata:
   - If Slot 1 is active (e.g. v1), the update targets Slot 2.
   - If both slots are valid (e.g. Slot 1 = v1, Slot 2 = v2), the update replaces whichever slot has the lower version number.
3. **Chunked Flashing:** Erases target sector, writes 128-byte chunks with CRC-32 verification per chunk, and writes updated metadata entry.
4. **Zero-Downtime Rollback:** If a new firmware update contains a regression, running `--rollback` toggles the active slot to the previous working version in Flash Sector 3 and boots immediately.

---

## 📁 Repository Structure

```
SecureBootLoader-OTA/
├── bootloader/                      # Bare-metal STM32CubeIDE Bootloader project
│   ├── Inc/
│   │   ├── bootloader.h            # Command codes, metadata structs, prototypes
│   │   └── systick.h               # Systick delay timer
│   ├── Src/
│   │   ├── bootloader.c            # Command handler, CRC, flash programming, jump & rollback
│   │   ├── main.c                  # System init, UART setup, boot timeout logic
│   │   └── systick.c               # SysTick driver
│   ├── drivers/                    # Register-level peripheral drivers
│   │   ├── Inc/                    # GPIO, USART, Flash, CRC, SPI headers & stm32f446xx.h
│   │   └── src/                    # Peripheral register driver implementations
│   ├── Startup/                    # Bare-metal startup assembly
│   └── STM32F446RETX_FLASH.ld      # Bootloader Linker Script (Sectors 0-2)
│
├── application/                     # Sample Application project
│   ├── Inc/ & Src/                 # Blinky application with version printing
│   ├── STM32F446RETX_FLASH.ld      # Linker script for Slot 1 (0x08010000)
│   ├── STM32F446RETX_FLASH_SLOT2.ld# Linker script for Slot 2 (0x08020000)
│   └── build_slots.py              # Automated script building versions for both slots
│
├── test_applications/               # Pre-compiled binaries for hardware verification
│   ├── app_blinking_led_slot1.bin  # Slot 1 application (Autonomous 250ms blink)
│   ├── app_blinking_led_slot2.bin  # Slot 2 application (Autonomous 250ms blink)
│   ├── app_button_led_slot1.bin    # Slot 1 application (Button PC13 controls LED PA5)
│   ├── app_button_led_slot2.bin    # Slot 2 application (Button PC13 controls LED PA5)
│   └── README.md                   # Full testing and verification guide
│
├── firmware_binaries/               # Production & benchmark release binaries
│   ├── app_blinking_led.bin        # Default Blinking LED binary
│   ├── app_button_led.bin          # Default Button-Controlled LED binary
│   ├── application_v1.bin          # Slot 1 benchmark application (200ms blink)
│   ├── application_v2.bin          # Slot 2 benchmark application (800ms blink)
│   └── application_v3.bin          # Slot 1 benchmark application (50ms blink)
│
├── boot.py                         # Python host CLI tool for UART flashing & management
└── README.md
```

---

## 💻 Host Tool Usage (`boot.py`)

The Python host tool (`boot.py`) handles all UART packet framing, hardware CRC-32 calculation, and slot management.

### Requirements
```bash
pip install pyserial
```

### 1. Query System & Slot Metadata
```bash
python boot.py --port COM6 --slot-info
```
Output:
```
==================================================
              DUAL-SLOT METADATA TABLE            
==================================================
  Active Slot : Slot 1 (Address: 0x08010000)
  Boot Status : Ready
--------------------------------------------------
  [SLOT 1] (0x08010000, Sector 4)
    Status  : VALID
    Version : v1
    Size    : 924 bytes
    CRC32   : 0xC316CEFE
--------------------------------------------------
  [SLOT 2] (0x08020000, Sector 5)
    Status  : VALID
    Version : v2
    Size    : 924 bytes
    CRC32   : 0x1AEB4527
==================================================
```

### 2. Verify CRC & Handshake
```bash
python boot.py --port COM6 --verify-crc
```

### 3. Update Firmware (Replaces Oldest Slot)
```bash
python boot.py --port COM6 --update firmware_binaries/application_v2.bin 2
```

### 4. Rollback to Previous Version
```bash
python boot.py --port COM6 --rollback
```

---

## 🧭 Development Roadmap

- [x] Register-level peripheral drivers (RCC, GPIO, USART2, Flash, CRC)
- [x] Bare-metal bootloader & vector table relocation (`SCB->VTOR`)
- [x] Split firmware memory maps & independent linker scripts
- [x] Framed UART packet protocol (`0xA5` sync frame)
- [x] Hardware CRC-32 validation on every packet
- [x] Dual-slot A/B flash management (Slot 1 & Slot 2)
- [x] Sector 3 non-volatile metadata storage
- [x] Automatic oldest-slot replacement algorithm
- [x] Fast rollback mechanism
- [x] Host CLI utility (`boot.py`)
- [ ] ESP32 Wi-Fi bridge for over-the-air (OTA) updates (Phase 6)
- [ ] Cryptographic firmware signing & ECDSA verification

---

## 📄 License
This project is open-source under the MIT License.
