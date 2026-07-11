# SecureBootLoader-OTA

Bare-metal secure bootloader + OTA firmware update system for the **STM32F446RE**, built from scratch in register-level Embedded C — no HAL.

> **Status:** 🚧 In Development

<details>
<summary><b>📄 Abstract (click to expand)</b></summary>

<br>

This project builds a secure firmware update system for the STM32F446RE entirely at the register level (bare-metal, no HAL/LL libraries). The goal is to understand and implement every layer of a bootloader — memory layout, flash programming, UART protocols, cryptographic verification, and rollback — before adding wireless OTA updates via an ESP32.

The firmware is split into two independent parts: a **Bootloader** and an **Application**. The bootloader decides which firmware to run, receives new firmware over UART, verifies it, and only then hands control to the application. If a new update is bad, the bootloader falls back to the last working version automatically.

</details>

<details>
<summary><b>🧭 Workflow / Development Phases (click each to expand)</b></summary>

<br>

<details>
<summary><b>Phase 1 — Bare-Metal Basics (Week 1)</b></summary>
<br>

Learn STM32F446RE at the register level before touching the bootloader.

- Clock configuration (RCC)
- GPIO via registers (no HAL)
- UART setup and communication via registers
- Linker scripts, startup files, memory map basics
- Reading the reference manual and datasheet directly

</details>

<details>
<summary><b>Phase 2 — Simple Bootloader (Laptop-flashed)</b></summary>
<br>

Build the first working bootloader, flashed directly via ST-Link from the laptop.

- Bootloader occupies a fixed flash region
- Bootloader jumps to application at a fixed address
- No update mechanism yet — just proving the jump works
- Understanding vector table relocation (VTOR)

</details>

<details>
<summary><b>Phase 3 — Splitting Firmware (Bootloader + Application)</b></summary>
<br>

Divide flash into two independent regions.

- Separate linker scripts for Bootloader and Application
- Bootloader region + Application region defined in flash
- Bootloader always runs first, then jumps to Application
- Each part is compiled and flashed as its own binary

</details>

<details>
<summary><b>Phase 4 — UART Firmware Update (PuTTY / CoolTerm)</b></summary>
<br>

Update the Application over UART instead of ST-Link.

- PC sends new firmware binary over UART using PuTTY or CoolTerm
- Bootloader receives the binary and writes it to flash
- Simple UART protocol: start byte, size, data, ACK/NACK
- Manual trigger to enter "update mode" on boot

</details>

<details>
<summary><b>Phase 5 — Verification + Rollback</b></summary>
<br>

Make updates safe.

- SHA-256 hash check on received firmware
- Digital signature check to confirm authenticity
- Firmware version check (blocks downgrade attacks)
- Dual-bank flash (A/B slots) — new firmware never overwrites the working copy
- Automatic rollback to last good firmware if new image fails checks or fails to boot

</details>

<details>
<summary><b>Phase 6 — OTA via ESP32 (Final)</b></summary>
<br>

Add wireless delivery on top of the same verified update pipeline.

- Python OTA server hosts firmware on the laptop
- ESP32 connects over Wi-Fi and downloads the firmware
- ESP32 forwards the firmware to STM32 over UART
- STM32 bootloader treats it exactly like Phase 4/5 — no logic change needed
- Single-bank OTA optimization using ESP32 storage as staging (stretch goal)

</details>

</details>

<details>
<summary><b>⚙️ Features</b></summary>
<br>

- Bare-metal STM32F446RE bootloader (register-level, no HAL)
- Dual-bank firmware update mechanism (A/B slots)
- UART-based firmware upload (PuTTY / CoolTerm)
- Secure boot process with pre-execution validation
- SHA-256 firmware integrity verification
- Digital signature verification
- Firmware version validation (anti-rollback)
- Automatic rollback on failed update
- OTA firmware updates via ESP32 (planned)

</details>

<details>
<summary><b>🗺️ Architecture</b></summary>
<br>

```
             Laptop
       (Python OTA Server)
               │
        Wi-Fi (HTTP/TCP)
               │
             ESP32
      (Communication Module)
               │ UART
               ▼
      +-----------------------+
      |   STM32 Bootloader    |
      |   (Bare-Metal, C)     |
      |------------------------|
      | Flash Driver           |
      | SHA-256 Verification   |
      | Signature Check        |
      | Version Check          |
      | Rollback Logic         |
      +-----------+-----------+
                  │
        Flash Memory (A/B Slots)
                  │
        Verified Application
```

</details>

<details>
<summary><b>✅ Roadmap</b></summary>
<br>

- [x] Project Planning
- [ ] Bare-Metal Basics (Clocks, GPIO, UART)
- [ ] Simple Bootloader + Jump-to-Application
- [ ] Split Bootloader / Application (Linker Scripts)
- [ ] UART Firmware Update (PuTTY/CoolTerm)
- [ ] Dual-Bank Flash Management
- [ ] SHA-256 Firmware Verification
- [ ] Digital Signature Verification
- [ ] Firmware Version Management
- [ ] Rollback Mechanism
- [ ] ESP32-Based OTA Update
- [ ] Single-Bank OTA Optimization

</details>

<details>
<summary><b>🛠️ Technologies</b></summary>
<br>

- STM32F446RE (Bare-Metal, register-level)
- Embedded C
- STM32CubeIDE / Makefile + GCC toolchain
- UART Communication (PuTTY / CoolTerm)
- Flash Memory Programming
- SHA-256
- Digital Signatures
- ESP32 (planned)
- OTA Firmware Update

</details>

<details>
<summary><b>🎯 Objectives</b></summary>
<br>

- Learn STM32 internals through bare-metal, register-level programming.
- Build a custom bootloader from scratch (no HAL).
- Implement authenticated, verifiable firmware updates.
- Prevent execution of tampered or unauthorized firmware.
- Support reliable rollback on failed updates.
- Enable wireless firmware updates using an ESP32.

</details>

---

**Note:** Every phase builds directly on the previous one — bare-metal fundamentals first, then a minimal bootloader, then splitting firmware, then UART updates, then security + rollback, and finally OTA over ESP32.
