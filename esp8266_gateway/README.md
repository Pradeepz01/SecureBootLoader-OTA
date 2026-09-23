# ESP8266 Wireless OTA Gateway for STM32F446RE Secure Bootloader

This firmware turns your **ESP8266** (NodeMCU, WeMos D1 Mini, or ESP-12E/F) into a **High-Speed Wireless OTA Web Portal & Gateway** for the STM32F446RE bare-metal bootloader.

---

## 1. Hardware Pin Interconnect (Wiring)

Connect the 3 signal lines between your **ESP8266 (NodeMCU)** and the **STM32F446RE (Nucleo-64)**:

| ESP8266 NodeMCU Pin | GPIO Number | Function | STM32F446RE Pin | STM32 Peripheral | Notes |
| :---: | :---: | :---: | :---: | :---: | :--- |
| **D5** | GPIO 14 | `TX` (Transmit) | **PA3** | USART2 `RX` | Cross-connected (D5 $\rightarrow$ PA3) |
| **D6** | GPIO 12 | `RX` (Receive) | **PA2** | USART2 `TX` | Cross-connected (D6 $\leftarrow$ PA2) |
| **GND** | GND | Ground | **GND** | Board Ground | **MANDATORY**: Common reference ground |
| **VIN / 5V** | 5V | Power | **5V** or USB | Power | Powers the ESP8266 board |

> [!CAUTION]
> **COMMON GROUND IS MANDATORY**: You must connect a jumper wire between **ESP8266 GND** and **STM32 GND**. Without common ground, the UART signal voltages float and communication will fail!

---

## 2. Flashing the ESP8266 on Your Friend's Laptop

### Step 1: Open Arduino IDE
Download and install [Arduino IDE](https://www.arduino.cc/en/software) if not already installed.

### Step 2: Add ESP8266 Board URL
1. In Arduino IDE, click **File $\rightarrow$ Preferences**.
2. In the **Additional boards manager URLs** field, add:
   ```text
   http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
3. Click **OK**.

### Step 3: Install the ESP8266 Package
1. Go to **Tools $\rightarrow$ Board $\rightarrow$ Boards Manager...**
2. Search for `esp8266` (by *ESP8266 Community*).
3. Click **Install** (version 3.1.2 or latest).

### Step 4: Configure Board Settings & Upload
1. Plug the ESP8266 into your friend's laptop with a USB data cable.
2. Under the **Tools** menu, configure:
   - **Board:** `NodeMCU 1.0 (ESP-12E Module)` *(or `Generic ESP8266 Module` / `LOLIN(WEMOS) D1 R2 & mini` depending on your board)*
   - **CPU Frequency:** `160 MHz` *(gives maximum speed for SoftwareSerial and web serving)*
   - **Upload Speed:** `921600` *(or `115200` if upload fails)*
   - **Port:** Select the COM port that appears for the ESP8266.
3. Open [`esp8266_ota_gateway.ino`](file:///d:/USEME%20FOLDER/ANTIGRAVITY/esp8266_gateway/esp8266_ota_gateway.ino).
4. Click the **Upload** arrow ($\rightarrow$).
5. Once uploaded, open the **Serial Monitor** at **115200 baud**. You will see:
   ```text
   =============================================
   STM32 OTA Gateway on ESP8266 Started
   Access Point Started: SSID='ESP8266-SecureBoot-OTA'
   AP IP Address: 192.168.4.1
   HTTP Web Server Started on Port 80
   =============================================
   ```

---

## 3. Controlling STM32 Wirelessly From Your Laptop

### Method A: Sleek Glassy Web Portal (Browser)
1. Power up both the ESP8266 and the STM32.
2. On your laptop or phone, connect to Wi-Fi:
   - **SSID:** `ESP8266-SecureBoot-OTA`
   - **Password:** `secureboot123`
3. Open your web browser and go to:
   👉 **`http://192.168.4.1`** (or `http://esp8266-ota.local`)
4. Features available in the portal:
   - **Dual-Slot Status:** View active/standby slot versions and monotonic update counter.
   - **OTA Upload:** Drag and drop any `.bin` firmware file and click **Upload Firmware (OTA)**.
   - **Instant Rollback (252 ms):** Swap active slot to fallback slot with 1 click.
   - **Jump to App:** Start the user application executing on the STM32.

---

### Method B: Python CLI Client
From PowerShell or Terminal on your laptop while connected to the `ESP8266-SecureBoot-OTA` Wi-Fi:

```powershell
# Query STM32 Bootloader & Slot Status
python esp32_gateway\ota_client.py --ip 192.168.4.1 status

# Wirelessly flash firmware (.bin) into STM32
python esp32_gateway\ota_client.py --ip 192.168.4.1 flash application\Debug\application.bin

# Trigger instant A/B slot rollback
python esp32_gateway\ota_client.py --ip 192.168.4.1 rollback

# Jump to active application
python esp32_gateway\ota_client.py --ip 192.168.4.1 jump
```
*(The `ota_client.py` script is 100% cross-compatible with both ESP32 and ESP8266!)*
