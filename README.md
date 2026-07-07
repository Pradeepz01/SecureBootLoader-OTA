# SecureBootLoader-OTA
                 Laptop
      (Python OTA Server)
                │
           WiFi (HTTP/TCP)
                │
              ESP32
      (Communication Module)
                │ UART
                ▼
      +-----------------------+
      |   STM32 Bootloader    |
      |-----------------------|
      | Flash Driver          |
      | SHA-256 Verification  |
      | Signature Check       |
      | Version Check         |
      | Rollback Logic        |
      +-----------+-----------+
                  │
          Flash Memory (A/B Slots)
                  │
          Verified Application
A secure firmware update framework for STM32 microcontrollers featuring a custom bootloader, secure firmware verification, rollback support, and Over-the-Air (OTA) update capability using an ESP32 communication module.

> **Project Status:** 🚧 In Development

## Features

- Custom STM32 Bootloader
- Dual-Bank Firmware Update Mechanism
- UART-Based Firmware Upload
- Secure Boot Process
- SHA-256 Firmware Integrity Verification
- Digital Signature Verification
- Firmware Version Validation
- Rollback to Previous Firmware on Failed Update
- OTA Firmware Updates via ESP32 (Planned)

## Project Roadmap

- [x] Project Planning
- [ ] Basic Bootloader
- [ ] UART Firmware Update
- [ ] Dual-Bank Flash Management
- [ ] Bootloader to Application Jump
- [ ] SHA-256 Firmware Verification
- [ ] Digital Signature Verification
- [ ] Firmware Version Management
- [ ] Rollback Mechanism
- [ ] ESP32-Based OTA Update
- [ ] Single-Bank OTA Optimization using ESP32 Storage

## Planned Architecture

```text
                Laptop
        (Firmware Server)
               │
          USB-UART / Wi-Fi
               │
            ESP32 (OTA)
               │
             UART
               │
        STM32 Bootloader
               │
    ┌──────────┴──────────┐
    │                     │
Application A      Application B
```

## Technologies

- STM32F446RE
- Embedded C
- STM32CubeIDE
- UART Communication
- Flash Memory Programming
- SHA-256
- Digital Signatures
- ESP32 (Planned)
- OTA Firmware Update

## Objectives

- Develop a secure custom bootloader from scratch.
- Implement authenticated firmware updates.
- Prevent execution of tampered firmware.
- Support reliable firmware rollback.
- Enable wireless firmware updates using an ESP32.

---

**Note:** Development begins with a UART-based dual-bank bootloader. Once the core bootloader is validated, the OTA framework will be integrated using an ESP32 as the firmware staging device, enabling secure wireless firmware updates while reducing STM32 flash memory requirements.
