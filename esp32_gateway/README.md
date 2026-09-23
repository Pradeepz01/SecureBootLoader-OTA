# ESP32 Wireless OTA Gateway for STM32F446RE Secure Bootloader

This module turns your ESP32 into a **High-Speed Wireless OTA Bridge & Web Dashboard** for your STM32F446RE bare-metal bootloader.

---

## 1. Hardware Pin Interconnect (Wiring)

Connect the 3 signal lines between the **ESP32** and the **STM32F446RE (Nucleo-64)**:

| ESP32 Pin | Function | STM32F446RE Pin | STM32 Peripheral | Notes |
| :---: | :---: | :---: | :---: | :--- |
| **GPIO 17** | `TX2` (Transmit) | **PA3** | USART2 `RX` | Cross-connected (TX $\rightarrow$ RX) |
| **GPIO 16** | `RX2` (Receive) | **PA2** | USART2 `TX` | Cross-connected (RX $\leftarrow$ TX) |
| **GND** | Ground | **GND** | Board Ground | **MANDATORY**: Common reference ground |
| **VIN / 5V** | Power (5V) | **5V / E5V** or USB | Power | Can be powered via USB or from STM32 5V |

> [!CAUTION]
> **COMMON GROUND IS MANDATORY**: You must connect a wire between **ESP32 GND** and **STM32 GND**. Without common ground, the UART signals will have floating reference voltages and corrupt all communication.

---

## 2. Flashing the ESP32 on Your Friend's Laptop

Follow these 4 simple steps to upload `esp32_ota_gateway.ino` to your ESP32 using Arduino IDE:

### Step 1: Install Arduino IDE
Download and install [Arduino IDE 2.x](https://www.arduino.cc/en/software).

### Step 2: Install ESP32 Board Package
1. In Arduino IDE, go to **File $\rightarrow$ Preferences**.
2. In **Additional boards manager URLs**, paste this URL:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Click **OK**.
4. Go to **Tools $\rightarrow$ Board $\rightarrow$ Boards Manager...**
5. Search for `esp32` (by *Espressif Systems*) and click **Install**.

### Step 3: Configure Board Settings
1. Connect the ESP32 to your friend's laptop via a micro-USB / USB-C data cable.
2. Under **Tools**, configure:
   - **Board:** `ESP32 Dev Module` (or `DOIT ESP32 DEVKIT V1`)
   - **Port:** Select the COM port that appears (e.g., `COM3`, `COM4`, etc.)
   - **Upload Speed:** `921600` (or `115200` if upload fails)
   - **Flash Frequency:** `80MHz`
   - **Partition Scheme:** `Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)`

### Step 4: Open & Upload
1. Open [`esp32_ota_gateway.ino`](file:///d:/USEME%20FOLDER/ANTIGRAVITY/esp32_gateway/esp32_ota_gateway.ino).
2. Click the **Upload** arrow ($\rightarrow$).
   *(If it gets stuck at `Connecting........_____.....`, press and hold the **BOOT** button on the ESP32 until the flashing progress percentage begins).*
3. Once done, it will say `Hard resetting via RTS pin...`. The ESP32 is ready!

---

## 3. How to Use & Control Your STM32 Wirelessly

Once the ESP32 is flashed and wired to the STM32:

### Method A: Sleek Web Portal (No Software Needed)
1. Power up both the ESP32 and STM32.
2. On your laptop or smartphone, open Wi-Fi settings and connect to:
   - **SSID:** `ESP32-SecureBoot-OTA`
   - **Password:** `secureboot123`
3. Open your browser and navigate to:
   👉 **`http://192.168.4.1`** (or `http://esp32-ota.local`)
4. The **Glassy Web Dashboard** will load:
   - View real-time Active & Standby slot versions.
   - Drag and drop your `.bin` firmware file and click **Upload Firmware (OTA)**.
   - Click **Instant Rollback** to swap slots in 252 ms.
   - Click **Jump to App** to launch user code.

---

### Method B: Python CLI Client (From Your Laptop)
While connected to the `ESP32-SecureBoot-OTA` Wi-Fi network, run [`ota_client.py`](file:///d:/USEME%20FOLDER/ANTIGRAVITY/esp32_gateway/ota_client.py):

```powershell
# 1. Check Bootloader & Dual-Slot Status:
python ota_client.py --ip 192.168.4.1 status

# 2. Upload and Flash New Firmware (.bin) Wirelessly:
python ota_client.py --ip 192.168.4.1 flash application\Debug\application.bin

# 3. Trigger Instant A/B Slot Rollback:
python ota_client.py --ip 192.168.4.1 rollback

# 4. Jump to the Active Application:
python ota_client.py --ip 192.168.4.1 jump
```
