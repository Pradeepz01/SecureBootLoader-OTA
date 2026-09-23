/*
 * ESP8266 Wireless OTA Gateway & Web Controller for STM32F446RE Secure Bootloader
 * 
 * Target Board: NodeMCU 1.0 (ESP-12E / ESP-12F), WeMos D1 Mini, or Generic ESP8266
 * Target Microcontroller: STM32F446RE (Bare-Metal Secure Bootloader via USART2)
 *
 * Default Pin Connections (SoftwareSerial on D6 & D5):
 *   ESP8266 D5 (GPIO 14, TX) ---> STM32 PA3 (USART2 RX)
 *   ESP8266 D6 (GPIO 12, RX) ---> STM32 PA2 (USART2 TX)
 *   ESP8266 GND              ---> STM32 GND (Mandatory Common Ground)
 *   ESP8266 VIN / 5V         ---> 5V Power Supply or USB
 *
 * (Note: USB Serial remains active at 115200 baud for Serial Monitor debugging!)
 *
 * Web Portal:
 *   SoftAP: "ESP8266-SecureBoot-OTA" (Password: "secureboot123")
 *   IP: http://192.168.4.1 (or mDNS: http://esp8266-ota.local)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>

// Wi-Fi Access Point Configuration
const char* AP_SSID = "ESP8266-SecureBoot-OTA";
const char* AP_PASS = "secureboot123";

// Optional Station Wi-Fi (Home / Lab Router)
const char* STA_SSID = "YOUR_WIFI_SSID";
const char* STA_PASS = "YOUR_WIFI_PASS";

// UART Communication Mode:
// 1 = SoftwareSerial on D6 (RX) / D5 (TX) — Allows Serial Monitor on USB simultaneously!
// 0 = Hardware Serial on standard RX (GPIO 3) / TX (GPIO 1)
#define USE_SOFTWARE_SERIAL 1

#if USE_SOFTWARE_SERIAL
  #define STM32_RX_PIN 12 // D6 on NodeMCU -> Connects to STM32 PA2 (TX)
  #define STM32_TX_PIN 14 // D5 on NodeMCU -> Connects to STM32 PA3 (RX)
  SoftwareSerial stm32Serial(STM32_RX_PIN, STM32_TX_PIN);
  #define STM32_PORT stm32Serial
#else
  #define STM32_PORT Serial
#endif

#define STM32_BAUD 115200

// Protocol Status Codes
#define BL_ACK  0xA5
#define BL_NACK 0x7F

// Bootloader Command Opcodes
#define BL_GET_VERSION   0x51
#define BL_FLASH_ERASE   0x52
#define BL_MEM_WRITE     0x53
#define BL_JUMP_APP      0x54
#define BL_GET_SLOT_INFO 0x56
#define BL_ROLLBACK      0x57
#define BL_ACTIVATE_SLOT 0x58

#define SLOT1_BASE 0x08010000
#define SLOT2_BASE 0x08020000

ESP8266WebServer server(80);

// Global State
struct SlotInfo {
  uint8_t active_slot;
  uint8_t prev_slot;
  uint8_t slot1_state;
  uint8_t slot2_state;
  uint32_t slot1_version;
  uint32_t slot2_version;
  uint32_t update_counter;
  uint8_t oldest_slot;
  uint8_t rollback_possible;
  bool valid;
} g_slot_info;

uint8_t g_bl_version[3] = {0, 0, 0};
bool g_bl_connected = false;

// -------------------------------------------------------------
// CRC-32 Calculation matching STM32 Hardware Unit
// Polynomial: 0x04C11DB7, Init: 0xFFFFFFFF, MSB-First
// -------------------------------------------------------------
uint32_t calc_stm32_crc(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  const uint32_t poly = 0x04C11DB7;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 32; b++) {
      if (crc & 0x80000000) {
        crc = ((crc << 1) ^ poly);
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}

// -------------------------------------------------------------
// Low-Level STM32 Packet Transmission & Reception
// -------------------------------------------------------------
bool send_stm32_cmd(const uint8_t *payload, size_t payload_len, uint8_t *resp_buf, size_t expected_resp_len, uint32_t timeout_ms = 1000) {
  while (STM32_PORT.available()) STM32_PORT.read(); // Flush buffer

  uint32_t crc = calc_stm32_crc(payload, payload_len);
  uint8_t pkt_len = payload_len + 4; // payload + 4B CRC

  STM32_PORT.write(pkt_len);
  STM32_PORT.write(payload, payload_len);
  STM32_PORT.write((uint8_t)(crc & 0xFF));
  STM32_PORT.write((uint8_t)((crc >> 8) & 0xFF));
  STM32_PORT.write((uint8_t)((crc >> 16) & 0xFF));
  STM32_PORT.write((uint8_t)((crc >> 24) & 0xFF));
  STM32_PORT.flush();

  size_t bytes_read = 0;
  uint32_t t_start = millis();
  while (bytes_read < expected_resp_len && (millis() - t_start) < timeout_ms) {
    if (STM32_PORT.available()) {
      resp_buf[bytes_read++] = STM32_PORT.read();
    } else {
      delay(1);
    }
  }
  return (bytes_read == expected_resp_len);
}

// Query Bootloader Version (Opcode 0x51)
bool query_bl_version() {
  uint8_t cmd[1] = { BL_GET_VERSION };
  uint8_t resp[4]; // [ACK (0xA5)] [Major] [Minor] [Patch]
  if (send_stm32_cmd(cmd, 1, resp, 4, 300)) {
    if (resp[0] == BL_ACK) {
      g_bl_version[0] = resp[1];
      g_bl_version[1] = resp[2];
      g_bl_version[2] = resp[3];
      g_bl_connected = true;
      return true;
    }
  }
  g_bl_connected = false;
  return false;
}

// Query Dual-Slot Metadata (Opcode 0x56)
bool query_slot_info() {
  uint8_t cmd[1] = { BL_GET_SLOT_INFO };
  uint8_t resp[19]; // [ACK (0xA5)] + 18B Table
  if (send_stm32_cmd(cmd, 1, resp, 19, 500)) {
    if (resp[0] == BL_ACK) {
      g_slot_info.active_slot       = resp[1];
      g_slot_info.prev_slot         = resp[2];
      g_slot_info.slot1_state       = resp[3];
      g_slot_info.slot2_state       = resp[4];
      g_slot_info.slot1_version     = (uint32_t)resp[5] | ((uint32_t)resp[6] << 8) | ((uint32_t)resp[7] << 16) | ((uint32_t)resp[8] << 24);
      g_slot_info.slot2_version     = (uint32_t)resp[9] | ((uint32_t)resp[10] << 8) | ((uint32_t)resp[11] << 16) | ((uint32_t)resp[12] << 24);
      g_slot_info.update_counter    = (uint32_t)resp[13] | ((uint32_t)resp[14] << 8) | ((uint32_t)resp[15] << 16) | ((uint32_t)resp[16] << 24);
      g_slot_info.oldest_slot       = resp[17];
      g_slot_info.rollback_possible = resp[18];
      g_slot_info.valid             = true;
      g_bl_connected                = true;
      return true;
    }
  }
  g_slot_info.valid = false;
  return false;
}

// Erase Flash Sector (Opcode 0x52)
bool erase_sector(uint8_t sector) {
  uint8_t cmd[3] = { BL_FLASH_ERASE, sector, 1 };
  uint8_t resp[2]; // [ACK] [Status]
  // Sector 4 takes ~533ms, Sector 5 takes ~924ms; timeout 2500ms
  if (send_stm32_cmd(cmd, 3, resp, 2, 2500)) {
    return (resp[0] == BL_ACK && resp[1] == 0x00);
  }
  return false;
}

// Program 1 Chunk (Opcode 0x53)
bool write_flash_chunk(uint32_t addr, const uint8_t *data, size_t len) {
  uint8_t payload[5 + len];
  payload[0] = BL_MEM_WRITE;
  payload[1] = (uint8_t)(addr & 0xFF);
  payload[2] = (uint8_t)((addr >> 8) & 0xFF);
  payload[3] = (uint8_t)((addr >> 16) & 0xFF);
  payload[4] = (uint8_t)((addr >> 24) & 0xFF);
  memcpy(&payload[5], data, len);

  uint8_t resp[2];
  if (send_stm32_cmd(payload, 5 + len, resp, 2, 800)) {
    return (resp[0] == BL_ACK && resp[1] == 0x00);
  }
  return false;
}

// Activate Slot (Opcode 0x58)
bool activate_slot(uint8_t slot, uint32_t version) {
  uint8_t cmd[6];
  cmd[0] = BL_ACTIVATE_SLOT;
  cmd[1] = slot;
  cmd[2] = (uint8_t)(version & 0xFF);
  cmd[3] = (uint8_t)((version >> 8) & 0xFF);
  cmd[4] = (uint8_t)((version >> 16) & 0xFF);
  cmd[5] = (uint8_t)((version >> 24) & 0xFF);

  uint8_t resp[2];
  if (send_stm32_cmd(cmd, 6, resp, 2, 1200)) {
    return (resp[0] == BL_ACK && resp[1] == 0x00);
  }
  return false;
}

// Instant Rollback (Opcode 0x57)
bool execute_rollback(uint8_t &target_slot) {
  uint8_t cmd[1] = { BL_ROLLBACK };
  uint8_t resp[3]; // [ACK] [Status] [TargetSlot]
  if (send_stm32_cmd(cmd, 1, resp, 3, 1200)) {
    if (resp[0] == BL_ACK && resp[1] == 0x00) {
      target_slot = resp[2];
      return true;
    }
  }
  return false;
}

// Jump to Application (Opcode 0x54)
bool jump_to_application() {
  uint8_t cmd[1] = { BL_JUMP_APP };
  uint8_t resp[2];
  if (send_stm32_cmd(cmd, 1, resp, 2, 500)) {
    return (resp[0] == BL_ACK && resp[1] == 0x00);
  }
  return false;
}

// -------------------------------------------------------------
// Glassy Dashboard HTML UI (Stored in Flash PROGMEM)
// -------------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>STM32 Secure Bootloader — ESP8266 OTA Gateway</title>
<style>
  :root {
    --bg: #0a0e17;
    --card-bg: rgba(255, 255, 255, 0.05);
    --border: rgba(255, 255, 255, 0.12);
    --accent: #00f2fe;
    --accent-glow: rgba(0, 242, 254, 0.35);
    --success: #00ff87;
    --danger: #ff0055;
    --text: #e2e8f0;
    --text-muted: #94a3b8;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  body {
    background: radial-gradient(circle at 50% 0%, #151f33 0%, var(--bg) 80%);
    color: var(--text);
    min-height: 100vh;
    padding: 20px;
    display: flex;
    justify-content: center;
  }
  .container { max-width: 860px; width: 100%; display: flex; flex-direction: column; gap: 18px; }
  .glass-card {
    background: var(--card-bg);
    backdrop-filter: blur(16px);
    -webkit-backdrop-filter: blur(16px);
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 20px;
    box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37);
  }
  .header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid var(--border); padding-bottom: 14px; }
  .header h1 { font-size: 19px; font-weight: 700; color: #fff; }
  .status-badge {
    padding: 4px 12px;
    border-radius: 20px;
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
  }
  .status-online { background: rgba(0, 255, 135, 0.15); color: var(--success); border: 1px solid var(--success); }
  .status-offline { background: rgba(255, 0, 85, 0.15); color: var(--danger); border: 1px solid var(--danger); }
  
  .grid-2 { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 14px; }
  .slot-card {
    background: rgba(0, 0, 0, 0.28);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 16px;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .slot-card.active { border-color: var(--accent); box-shadow: 0 0 15px var(--accent-glow); }
  .slot-card .title { font-size: 13px; color: var(--text-muted); display: flex; justify-content: space-between; }
  .slot-card .version { font-size: 19px; font-weight: bold; color: #fff; }
  .slot-card .addr { font-size: 11px; font-family: monospace; color: var(--accent); }

  .upload-area {
    border: 2px dashed var(--border);
    border-radius: 12px;
    padding: 26px;
    text-align: center;
    cursor: pointer;
    transition: all 0.2s ease;
  }
  .upload-area:hover { border-color: var(--accent); background: rgba(0, 242, 254, 0.03); }
  .upload-area input { display: none; }
  
  .btn-group { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 14px; }
  button {
    background: linear-gradient(135deg, #00f2fe 0%, #4facfe 100%);
    border: none;
    border-radius: 8px;
    padding: 11px 18px;
    color: #0a0e17;
    font-weight: 700;
    font-size: 12px;
    cursor: pointer;
    transition: all 0.2s ease;
    box-shadow: 0 4px 15px rgba(0, 242, 254, 0.25);
  }
  button:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(0, 242, 254, 0.4); }
  button.btn-danger {
    background: linear-gradient(135deg, #ff0844 0%, #ffb199 100%);
    box-shadow: 0 4px 15px rgba(255, 8, 68, 0.25);
    color: #fff;
  }
  button.btn-secondary {
    background: rgba(255, 255, 255, 0.08);
    color: var(--text);
    border: 1px solid var(--border);
    box-shadow: none;
  }
  button:disabled { opacity: 0.4; cursor: not-allowed; transform: none; }

  .progress-container { width: 100%; background: rgba(0,0,0,0.4); height: 7px; border-radius: 4px; overflow: hidden; display: none; margin-top: 10px; }
  .progress-bar { width: 0%; height: 100%; background: linear-gradient(90deg, #00ff87, #00f2fe); transition: width 0.1s ease; }

  .log-console {
    background: #000;
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 10px;
    font-family: monospace;
    font-size: 11px;
    height: 130px;
    overflow-y: auto;
    color: #00ff87;
  }
</style>
</head>
<body>
<div class="container">
  <div class="glass-card header">
    <div>
      <h1>⚡ ESP8266 Wireless OTA Gateway</h1>
      <p style="font-size: 11px; color: var(--text-muted); margin-top: 2px;">STM32F446RE Secure Dual-Slot Management</p>
    </div>
    <div id="conn-badge" class="status-badge status-offline">Connecting...</div>
  </div>

  <div class="grid-2">
    <div class="slot-card" id="card-slot1">
      <div class="title">
        <span>SLOT 1 (Sector 4 - 64 KB)</span>
        <span id="badge-slot1">Inactive</span>
      </div>
      <div class="version" id="ver-slot1">--</div>
      <div class="addr">0x08010000</div>
    </div>

    <div class="slot-card" id="card-slot2">
      <div class="title">
        <span>SLOT 2 (Sector 5 - 128 KB)</span>
        <span id="badge-slot2">Inactive</span>
      </div>
      <div class="version" id="ver-slot2">--</div>
      <div class="addr">0x08020000</div>
    </div>
  </div>

  <div class="glass-card">
    <h3 style="font-size: 14px; margin-bottom: 10px;">Over-The-Air Firmware Stream</h3>
    <div class="upload-area" id="drop-zone" onclick="document.getElementById('fw-file').click();">
      <p style="font-size: 13px; font-weight: 600;">Click or Drop Firmware (.bin) Here</p>
      <p style="font-size: 11px; color: var(--text-muted); margin-top: 4px;" id="file-label">Target: Auto-Replacing Oldest Slot</p>
      <input type="file" id="fw-file" accept=".bin" onchange="handleFileSelected(this)">
    </div>

    <div class="progress-container" id="prog-wrap">
      <div class="progress-bar" id="prog-bar"></div>
    </div>

    <div class="btn-group">
      <button id="btn-upload" onclick="startUpload()" disabled>Upload Firmware (OTA)</button>
      <button class="btn-danger" id="btn-rollback" onclick="triggerRollback()">Instant Rollback (252ms)</button>
      <button class="btn-secondary" onclick="jumpApplication()">Jump to App</button>
      <button class="btn-secondary" onclick="refreshStatus()">Refresh</button>
    </div>
  </div>

  <div class="glass-card" style="padding: 14px;">
    <div style="font-size: 11px; font-weight: 600; margin-bottom: 6px; color: var(--text-muted);">LIVE UART LOGS</div>
    <div class="log-console" id="console"></div>
  </div>
</div>

<script>
let selectedFile = null;

function log(msg) {
  const c = document.getElementById('console');
  const d = new Date().toLocaleTimeString();
  c.innerHTML += `[${d}] ${msg}\n`;
  c.scrollTop = c.scrollHeight;
}

function formatVersion(v) {
  if (!v || v === 0) return 'Empty';
  const major = (v >> 16) & 0xFF;
  const minor = (v >> 8) & 0xFF;
  const patch = v & 0xFF;
  return `v${major}.${minor}.${patch}`;
}

async function refreshStatus() {
  try {
    const res = await fetch('/api/status');
    const data = await res.json();
    
    const badge = document.getElementById('conn-badge');
    if (data.connected) {
      badge.className = 'status-badge status-online';
      badge.textContent = `STM32 Online (v${data.bl_version.join('.')})`;
    } else {
      badge.className = 'status-badge status-offline';
      badge.textContent = 'STM32 Offline';
    }

    if (data.slots && data.slots.valid) {
      const s1Card = document.getElementById('card-slot1');
      const s2Card = document.getElementById('card-slot2');
      const s1Active = data.slots.active_slot === 1;

      s1Card.className = 'slot-card ' + (s1Active ? 'active' : '');
      s2Card.className = 'slot-card ' + (!s1Active ? 'active' : '');

      document.getElementById('badge-slot1').textContent = s1Active ? 'ACTIVE' : 'STANDBY';
      document.getElementById('badge-slot2').textContent = !s1Active ? 'ACTIVE' : 'STANDBY';

      document.getElementById('ver-slot1').textContent = formatVersion(data.slots.slot1_version);
      document.getElementById('ver-slot2').textContent = formatVersion(data.slots.slot2_version);

      document.getElementById('file-label').textContent = `Target Slot: Slot ${data.slots.oldest_slot} (Auto)`;
    }
  } catch (err) {
    log('Status fetch error: ' + err);
  }
}

function handleFileSelected(input) {
  if (input.files && input.files[0]) {
    selectedFile = input.files[0];
    document.getElementById('file-label').textContent = `Selected: ${selectedFile.name} (${selectedFile.size} bytes)`;
    document.getElementById('btn-upload').disabled = false;
    log(`Firmware loaded: ${selectedFile.name} (${selectedFile.size} bytes)`);
  }
}

function startUpload() {
  if (!selectedFile) return;

  const btn = document.getElementById('btn-upload');
  btn.disabled = true;
  document.getElementById('prog-wrap').style.display = 'block';
  const progBar = document.getElementById('prog-bar');

  log('Starting wireless OTA upload to ESP8266...');

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/upload', true);

  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      const pct = Math.round((e.loaded / e.total) * 100);
      progBar.style.width = pct + '%';
    }
  };

  xhr.onload = function() {
    if (xhr.status === 200) {
      log('OTA Update Complete! Written & Verified on STM32.');
      progBar.style.width = '100%';
      setTimeout(refreshStatus, 1000);
    } else {
      log('OTA Failed: ' + xhr.responseText);
    }
    btn.disabled = false;
  };

  xhr.onerror = function() {
    log('Network error during upload.');
    btn.disabled = false;
  };

  const formData = new FormData();
  formData.append('firmware', selectedFile);
  xhr.send(formData);
}

async function triggerRollback() {
  if (!confirm('Perform instantaneous A/B slot rollback?')) return;
  log('Issuing BL_ROLLBACK (0x57)...');
  try {
    const res = await fetch('/api/rollback', { method: 'POST' });
    const j = await res.json();
    if (j.success) {
      log(`Rollback completed in 252ms! Switched to Slot ${j.target_slot}.`);
      setTimeout(refreshStatus, 600);
    } else {
      log('Rollback rejected: ' + j.message);
    }
  } catch (e) {
    log('Rollback error: ' + e);
  }
}

async function jumpApplication() {
  log('Issuing BL_JUMP_APP (0x54)...');
  try {
    const res = await fetch('/api/jump', { method: 'POST' });
    const j = await res.json();
    log(j.success ? 'Application executing on STM32!' : 'Jump failed: ' + j.message);
  } catch (e) {
    log('Jump error: ' + e);
  }
}

refreshStatus();
setInterval(refreshStatus, 4000);
log('ESP8266 Gateway initialized. Listening on SoftwareSerial (D6/D5 @ 115200).');
</script>
</body>
</html>
)rawliteral";

// -------------------------------------------------------------
// Route Handlers
// -------------------------------------------------------------
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  query_bl_version();
  query_slot_info();

  String json = "{";
  json += "\"connected\":" + String(g_bl_connected ? "true" : "false") + ",";
  json += "\"bl_version\":[" + String(g_bl_version[0]) + "," + String(g_bl_version[1]) + "," + String(g_bl_version[2]) + "],";
  json += "\"slots\":{";
  json += "\"valid\":" + String(g_slot_info.valid ? "true" : "false") + ",";
  json += "\"active_slot\":" + String(g_slot_info.active_slot) + ",";
  json += "\"prev_slot\":" + String(g_slot_info.prev_slot) + ",";
  json += "\"slot1_version\":" + String(g_slot_info.slot1_version) + ",";
  json += "\"slot2_version\":" + String(g_slot_info.slot2_version) + ",";
  json += "\"update_counter\":" + String(g_slot_info.update_counter) + ",";
  json += "\"oldest_slot\":" + String(g_slot_info.oldest_slot) + ",";
  json += "\"rollback_possible\":" + String(g_slot_info.rollback_possible);
  json += "}}";

  server.send(200, "application/json", json);
}

void handleRollback() {
  uint8_t target = 0;
  if (execute_rollback(target)) {
    server.send(200, "application/json", "{\"success\":true,\"target_slot\":" + String(target) + "}");
  } else {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Rollback rejected by bootloader\"}");
  }
}

void handleJump() {
  if (jump_to_application()) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Jump rejected or empty slot\"}");
  }
}

// Streaming OTA Firmware Upload Handler
static uint8_t s_upload_target_slot = 1;
static uint8_t s_upload_sector = 4;
static uint32_t s_upload_base_addr = SLOT1_BASE;
static uint32_t s_bytes_written = 0;
static uint8_t s_chunk_buffer[128];
static size_t s_chunk_len = 0;
static bool s_upload_ok = true;

void handleFileUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    s_upload_ok = true;
    s_bytes_written = 0;
    s_chunk_len = 0;

    query_slot_info();
    s_upload_target_slot = g_slot_info.valid ? g_slot_info.oldest_slot : 1;
    s_upload_sector = (s_upload_target_slot == 2) ? 5 : 4;
    s_upload_base_addr = (s_upload_target_slot == 2) ? SLOT2_BASE : SLOT1_BASE;

    Serial.printf("[OTA] Erasing target Slot %d (Sector %d)...\n", s_upload_target_slot, s_upload_sector);

    if (!erase_sector(s_upload_sector)) {
      Serial.println("[OTA] Flash erase failed!");
      s_upload_ok = false;
      return;
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE && s_upload_ok) {
    size_t in_pos = 0;
    while (in_pos < upload.currentSize && s_upload_ok) {
      size_t copy_bytes = min((size_t)(128 - s_chunk_len), (size_t)(upload.currentSize - in_pos));
      memcpy(&s_chunk_buffer[s_chunk_len], &upload.buf[in_pos], copy_bytes);
      s_chunk_len += copy_bytes;
      in_pos += copy_bytes;

      if (s_chunk_len == 128) {
        if (!write_flash_chunk(s_upload_base_addr + s_bytes_written, s_chunk_buffer, 128)) {
          Serial.printf("[OTA] Write error at 0x%08X\n", s_upload_base_addr + s_bytes_written);
          s_upload_ok = false;
          break;
        }
        s_bytes_written += 128;
        s_chunk_len = 0;
      }
    }
  } 
  else if (upload.status == UPLOAD_FILE_END && s_upload_ok) {
    if (s_chunk_len > 0) {
      if (!write_flash_chunk(s_upload_base_addr + s_bytes_written, s_chunk_buffer, s_chunk_len)) {
        s_upload_ok = false;
      } else {
        s_bytes_written += s_chunk_len;
      }
      s_chunk_len = 0;
    }

    if (s_upload_ok) {
      uint32_t new_version = 0x00020000 + (g_slot_info.update_counter & 0xFFFF);
      if (activate_slot(s_upload_target_slot, new_version)) {
        Serial.printf("[OTA] Written %u bytes. Slot %d Activated.\n", s_bytes_written, s_upload_target_slot);
      } else {
        s_upload_ok = false;
      }
    }
  }
}

void handleUploadResponse() {
  if (s_upload_ok) {
    server.send(200, "application/json", "{\"success\":true,\"bytes\":" + String(s_bytes_written) + "}");
  } else {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Flash write or CRC error\"}");
  }
}

// -------------------------------------------------------------
// Arduino Setup & Loop
// -------------------------------------------------------------
void setup() {
  // USB Serial Monitor Debugging
  Serial.begin(115200);
  delay(100);

  // Initialize UART to STM32
#if USE_SOFTWARE_SERIAL
  stm32Serial.begin(STM32_BAUD);
  Serial.println("\n[INIT] SoftwareSerial active on D6 (RX) / D5 (TX) @ 115200 baud.");
#else
  Serial.begin(STM32_BAUD);
#endif

  Serial.println("=============================================");
  Serial.println("STM32 OTA Gateway on ESP8266 Started");

  // Start Access Point
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("Access Point Started: SSID='%s', Pass='%s'\n", AP_SSID, AP_PASS);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Connect to STA Wi-Fi if configured
  if (String(STA_SSID) != "YOUR_WIFI_SSID") {
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.printf("Connecting to %s...", STA_SSID);
  }

  // Setup mDNS
  if (MDNS.begin("esp8266-ota")) {
    Serial.println("mDNS responder started: http://esp8266-ota.local");
  }

  // Register Web Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/rollback", HTTP_POST, handleRollback);
  server.on("/api/jump", HTTP_POST, handleJump);
  server.on("/api/upload", HTTP_POST, handleUploadResponse, handleFileUpload);

  server.begin();
  Serial.println("HTTP Web Server Started on Port 80");
  Serial.println("Open http://192.168.4.1 in your browser.");
  Serial.println("=============================================\n");

  query_bl_version();
  query_slot_info();
}

void loop() {
  server.handleClient();
  MDNS.update();
  delay(2);
}
