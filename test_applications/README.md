# 🧪 Test Applications for Dual-Bank Secure Bootloader & OTA

This directory contains pre-compiled, hardware-validated binary firmware files (`.bin`) and source code designed to provide immediate, unambiguous visual verification of OTA updates and rollback operations on the **STM32F446RE Nucleo-64** board.

---

## 📦 Application Overview

| Application | Target Hardware | Observable Behavior | Slot 1 Binary (Base `0x08010000`) | Slot 2 Binary (Base `0x08020000`) |
| :--- | :--- | :--- | :--- | :--- |
| **1. Autonomous Blinking LED** | Green LED (`PA5` / LD2) | **Continuous Periodic Flash** at 250 ms (4 Hz toggle / 2 Hz cycle). Runs autonomously without touching any button. | `app_blinking_led_slot1.bin` *(924 B)* | `app_blinking_led_slot2.bin` *(924 B)* |
| **2. Push-Button Controlled LED** | Button (`PC13` / B1) + Green LED (`PA5` / LD2) | **User Interaction Driven**: Emits 3 quick greeting pulses at startup, then stays completely OFF. When Blue Button `B1` is pressed, Green LED lights up; when released, LED turns OFF. | `app_button_led_slot1.bin` *(968 B)* | `app_button_led_slot2.bin` *(968 B)* |

---

## 🚀 How to Flash & Observe Changes

### Method A: Over-The-Air via ESP8266 / ESP32 Wireless Gateway (Web Portal)

1. Connect your phone or laptop to the gateway Wi-Fi AP (`STM32-OTA-Gateway` / `SecureOTA-STM32`) or open the assigned DHCP IP in your browser:
   ```text
   http://192.168.4.1/
   ```
2. Check the live dashboard under **Slot Status**:
   - If the dashboard displays `Target: Slot 1 (Oldest)` → select **`app_blinking_led_slot1.bin`** or **`app_button_led_slot1.bin`**.
   - If the dashboard displays `Target: Slot 2 (Oldest)` → select **`app_blinking_led_slot2.bin`** or **`app_button_led_slot2.bin`**.
3. Click **Upload Firmware (OTA)**.
4. **Observe the Physical Board**:
   - If you flashed **Blinking LED**: The green LED on `PA5` will immediately start blinking steadily.
   - If you flashed **Button-Controlled LED**: The green LED will give 3 fast initial greeting blinks, then remain OFF. Now push the blue user button (`PC13`)—the green LED turns ON while held and turns OFF when released!

---

### Method B: Wired Serial Flashing via Python Host Tool (`boot.py`)

Connect the STM32 via USB (Virtual COM Port, default `COM6` at 115200 baud):

#### 1. Inspect Current Slot Configuration
```powershell
python boot.py --slot-info
```

#### 2. Update to Blinking LED Application (Replacing Oldest Slot)
```powershell
python boot.py --update test_applications/app_blinking_led_slot1.bin 1.0.1
```

#### 3. Update to Button-Controlled LED Application (Replacing Oldest Slot)
```powershell
python boot.py --update test_applications/app_button_led_slot2.bin 2.0.1
```

#### 4. Instant A/B Rollback Demo (252 ms Execution)
Switch back and forth between the two applications without re-flashing:
```powershell
python boot.py --rollback
```

---

## 🛠️ Compiling from Source

Both test applications are compiled with register-level bare-metal code (zero HAL overhead) using the ARM GNU Toolchain (`arm-none-eabi-gcc`):

- **Blinking LED Source**: `application/Src/main_blink.c`
- **Button Controlled LED Source**: `application/Src/main_button.c`
- **Build & Linker Orchestrator**: `application/build_slots.py`

To recompile all binaries for both flash slots:
```powershell
cd application
python build_slots.py
```
This builds all four target combinations (`slot1` and `slot2` for both applications) and automatically distributes them to `test_applications/` and `firmware_binaries/`.
