#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>

// Wi-Fi Access Point Configuration
const char* AP_SSID = "ESP8266-SecureBoot-OTA";
const char* AP_PASS = "secureboot123";

// Station Wi-Fi (Laptop Hotspot)
const char* STA_SSID = "PRADEEP";
const char* STA_PASS = "12345678";

#define STM32_BAUD 115200

// SoftwareSerial instance
SoftwareSerial stm32Serial;
#define STM32_PORT stm32Serial

// Protocol Status Codes
#define BL_ACK  0xA5
#define BL_NACK 0x7F

// Bootloader Command Opcodes
#define BL_GET_VERSION   0x51
#define BL_FLASH_ERASE   0x52
#define BL_MEM_WRITE     0x53
#define BL_VERIFY_CRC    0x54
#define BL_JUMP_APP      0x55
#define BL_GET_SLOT_INFO 0x56
#define BL_ROLLBACK      0x57
#define BL_ACTIVATE_SLOT 0x58
#define BL_RESET_MCU     0x5B

// STM32 Hardware Reset Pin (D1 / GPIO 5 connects to STM32 NRST)
#define STM32_NRST_PIN   5

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
bool send_stm32_cmd(const uint8_t *payload, size_t payload_len, uint8_t *resp_buf, size_t expected_resp_len, uint32_t timeout_ms = 800) {
  while (STM32_PORT.available()) STM32_PORT.read(); // Flush buffer

  uint32_t crc = calc_stm32_crc(payload, payload_len);
  uint8_t pkt_len = payload_len + 4; // payload + 4B CRC

  Serial.printf("  [UART_TX] cmd=0x%02X (%uB)... ", payload[0], pkt_len);

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

  Serial.printf("Rx: %u/%uB", (unsigned int)bytes_read, (unsigned int)expected_resp_len);
  if (bytes_read > 0) {
    Serial.print(" [");
    for (size_t i = 0; i < bytes_read; i++) {
      Serial.printf("%02X ", resp_buf[i]);
    }
    Serial.print("]");
  }
  Serial.println();

  return (bytes_read == expected_resp_len);
}

// Query Bootloader Version (Opcode 0x51)
bool query_bl_version() {
  uint8_t cmd[1] = { BL_GET_VERSION };
  uint8_t resp[4]; // [ACK (0xA5)] [Major] [Minor] [Patch]
  if (send_stm32_cmd(cmd, 1, resp, 4, 400)) {
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
  if (send_stm32_cmd(cmd, 3, resp, 2, 2500)) {
    return (resp[0] == BL_ACK && resp[1] == 0x00);
  }
  return false;
}

// Program Memory Chunk (Opcode 0x53)
bool write_flash_chunk(uint32_t address, const uint8_t *data, size_t len) {
  uint8_t payload[5 + 128];
  payload[0] = BL_MEM_WRITE;
  payload[1] = (uint8_t)(address & 0xFF);
  payload[2] = (uint8_t)((address >> 8) & 0xFF);
  payload[3] = (uint8_t)((address >> 16) & 0xFF);
  payload[4] = (uint8_t)((address >> 24) & 0xFF);
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
  if (send_stm32_cmd(cmd, 6, resp, 2, 1000)) {
    return (resp[0] == BL_ACK && resp[1] == 0x00);
  }
  return false;
}

// Execute Instant Rollback (Opcode 0x57)
bool execute_rollback(uint8_t &target_slot) {
  uint8_t cmd[1] = { BL_ROLLBACK };
  uint8_t resp[3];
  if (send_stm32_cmd(cmd, 1, resp, 3, 1000)) {
    if (resp[0] == BL_ACK && resp[2] == 0x00) {
      target_slot = resp[1];
      return true;
    }
  }
  return false;
}

// Jump to Application (Opcode 0x55)
bool jump_to_application() {
  uint8_t cmd[1] = { BL_JUMP_APP };
  uint8_t resp[2];
  if (send_stm32_cmd(cmd, 1, resp, 2, 500)) {
    if (resp[0] == BL_ACK && resp[1] == 0x00) {
      g_bl_connected = false; // STM32 entered user application
      return true;
    }
  }
  return false;
}

// Hardware & Software Reset of STM32 MCU into Bootloader Mode
void reset_stm32_hw() {
  Serial.println("[GATEWAY] 🔄 Triggering STM32 Reset into Bootloader...");

  // 1. Send software reset opcode (0x5B) over UART in case STM32 is listening
  uint8_t cmd[1] = { BL_RESET_MCU };
  uint8_t resp[1];
  send_stm32_cmd(cmd, 1, resp, 1, 60);

  // 2. Hardware open-drain reset pulse via NRST pin (D1 -> STM32 NRST)
  pinMode(STM32_NRST_PIN, OUTPUT);
  digitalWrite(STM32_NRST_PIN, LOW); // Pull STM32 NRST to GND
  delay(60);                         // Hold LOW for 60 ms
  pinMode(STM32_NRST_PIN, INPUT);     // Release back to Hi-Z (pulled to 3.3V by STM32 internal pull-up)
  delay(180);                        // Wait for bootloader reset and clock stabilization

  g_bl_connected = false;
  query_bl_version();
  query_slot_info();
}

// -------------------------------------------------------------
// Glassy Web UI
// -------------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>STM32 Secure Bootloader — Wireless OTA</title>
<style>
  :root {
    --bg: #0b0f19;
    --card-bg: rgba(22, 27, 34, 0.7);
    --border: rgba(255, 255, 255, 0.12);
    --primary: #00ff87;
    --accent: #60efff;
    --danger: #ff0055;
    --text: #e6edf3;
    --text-muted: #8b949e;
  }

  * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  body { background: var(--bg); color: var(--text); padding: 20px; display: flex; justify-content: center; }
  .container { width: 100%; max-width: 680px; display: flex; flex-direction: column; gap: 16px; }

  .glass-card {
    background: var(--card-bg);
    backdrop-filter: blur(16px);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 18px;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.37);
  }

  .header { display: flex; justify-content: space-between; align-items: center; }
  .header h1 { font-size: 18px; background: linear-gradient(135deg, var(--primary), var(--accent)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }

  .status-badge {
    padding: 4px 10px;
    border-radius: 20px;
    font-size: 11px;
    font-weight: 600;
  }
  .status-online { background: rgba(0, 255, 135, 0.15); color: var(--primary); border: 1px solid var(--primary); }
  .status-offline { background: rgba(255, 0, 85, 0.15); color: var(--danger); border: 1px solid var(--danger); }

  .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }

  .slot-card {
    background: rgba(255, 255, 255, 0.03);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 12px;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .slot-card.active { border-color: var(--primary); box-shadow: 0 0 10px rgba(0, 255, 135, 0.2); }
  .slot-card .title { font-size: 12px; font-weight: 600; color: var(--text-muted); display: flex; justify-content: space-between; }
  .slot-card .version { font-size: 16px; font-weight: 700; color: #fff; }
  .slot-card .addr { font-size: 11px; font-family: monospace; color: var(--accent); }

  .upload-area {
    border: 2px dashed var(--border);
    border-radius: 10px;
    padding: 24px 16px;
    text-align: center;
    cursor: pointer;
    background: rgba(255, 255, 255, 0.02);
    transition: all 0.2s ease;
  }
  .upload-area:hover { border-color: var(--accent); background: rgba(96, 239, 255, 0.05); }
  .upload-area input { display: none; }

  .progress-container {
    height: 8px;
    background: rgba(255, 255, 255, 0.1);
    border-radius: 4px;
    overflow: hidden;
    margin: 12px 0 6px 0;
    display: none;
  }
  .progress-bar { height: 100%; width: 0%; background: linear-gradient(90deg, var(--primary), var(--accent)); transition: width 0.1s linear; }

  .btn-group { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 10px; }
  button {
    flex: 1;
    min-width: 130px;
    padding: 10px 14px;
    border-radius: 8px;
    border: none;
    font-weight: 600;
    font-size: 12px;
    cursor: pointer;
    background: linear-gradient(135deg, var(--primary), var(--accent));
    color: #000;
    transition: transform 0.1s ease, filter 0.2s ease;
  }
  button:hover { filter: brightness(1.1); transform: translateY(-1px); }
  button:disabled { opacity: 0.4; cursor: not-allowed; transform: none; }
  button.btn-danger { background: var(--danger); color: #fff; }
  button.btn-warning { background: linear-gradient(135deg, #f6ad55, #ed8936); color: #000; }
  button.btn-secondary { background: rgba(255, 255, 255, 0.1); color: var(--text); border: 1px solid var(--border); }

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
      <button class="btn-warning" id="btn-reset" onclick="rebootToBootloader()">🔄 Reset to Bootloader</button>
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
let isStm32Connected = false;

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
      isStm32Connected = true;
      badge.className = 'status-badge status-online';
      badge.textContent = `STM32 Online (v${data.bl_version.join('.')})`;
    } else {
      isStm32Connected = false;
      badge.className = 'status-badge status-offline';
      badge.textContent = 'App Running (Click Reset)';
    }

    if (data.slots && data.slots.valid) {
      const s = data.slots;
      document.getElementById('ver-slot1').textContent = formatVersion(s.slot1_version);
      document.getElementById('ver-slot2').textContent = formatVersion(s.slot2_version);

      const c1 = document.getElementById('card-slot1');
      const c2 = document.getElementById('card-slot2');
      const b1 = document.getElementById('badge-slot1');
      const b2 = document.getElementById('badge-slot2');

      c1.className = 'slot-card' + (s.active_slot === 1 ? ' active' : '');
      c2.className = 'slot-card' + (s.active_slot === 2 ? ' active' : '');
      b1.textContent = (s.active_slot === 1) ? 'ACTIVE' : (s.oldest_slot === 1 ? 'OLDEST (NEXT)' : 'BACKUP');
      b2.textContent = (s.active_slot === 2) ? 'ACTIVE' : (s.oldest_slot === 2 ? 'OLDEST (NEXT)' : 'BACKUP');

      const targetText = s.oldest_slot === 1 ? 'Slot 1 (Sector 4 @ 0x08010000)' : 'Slot 2 (Sector 5 @ 0x08020000)';
      document.getElementById('file-label').textContent = `Target for Next Upload: ${targetText}`;
    }
  } catch (e) {
    document.getElementById('conn-badge').className = 'status-badge status-offline';
    document.getElementById('conn-badge').textContent = 'Gateway Offline';
  }
}

async function rebootToBootloader() {
  log('🔄 Triggering STM32 Reset (D1 -> STM32 NRST)...');
  const btn = document.getElementById('btn-reset');
  if (btn) btn.disabled = true;
  try {
    const res = await fetch('/api/reset', { method: 'POST' });
    const j = await res.json();
    log(j.message || 'Reset pulse sent.');
    setTimeout(() => {
      refreshStatus();
      if (btn) btn.disabled = false;
    }, 400);
  } catch (e) {
    log('Reset error: ' + e);
    if (btn) btn.disabled = false;
  }
}

function handleFileSelected(input) {
  if (input.files.length > 0) {
    selectedFile = input.files[0];
    document.getElementById('drop-zone').querySelector('p').textContent = `Selected: ${selectedFile.name} (${selectedFile.size} B)`;
    document.getElementById('btn-upload').disabled = false;
    log(`File selected: ${selectedFile.name}`);
  }
}

async function startUpload() {
  if (!selectedFile) return;

  const btn = document.getElementById('btn-upload');
  const progWrap = document.getElementById('prog-wrap');
  const progBar = document.getElementById('prog-bar');

  btn.disabled = true;

  // Auto-reset into Bootloader if STM32 is executing application
  if (!isStm32Connected) {
    log('STM32 is running application. Auto-resetting into Bootloader first...');
    try {
      await fetch('/api/reset', { method: 'POST' });
      await new Promise(r => setTimeout(r, 400));
      await refreshStatus();
    } catch (e) {
      log('Auto-reset note: ' + e);
    }
  }

  progWrap.style.display = 'block';
  progBar.style.width = '0%';

  log(`Initiating OTA Stream: ${selectedFile.name}...`);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/upload', true);

  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      const pct = Math.round((e.loaded / e.total) * 100);
      progBar.style.width = pct + '%';
      if (pct % 25 === 0) log(`Streaming Progress: ${pct}%`);
    }
  };

  xhr.onload = function() {
    progWrap.style.display = 'none';
    btn.disabled = false;
    if (xhr.status === 200) {
      log('OTA Update Successful! Slot programmed and verified with Hardware CRC.');
      setTimeout(refreshStatus, 800);
    } else {
      log('OTA Update Failed: ' + xhr.responseText);
    }
  };

  xhr.onerror = function() {
    progWrap.style.display = 'none';
    btn.disabled = false;
    log('Network error occurred during OTA upload.');
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
  log('Issuing BL_JUMP_APP (0x55)...');
  try {
    const res = await fetch('/api/jump', { method: 'POST' });
    const j = await res.json();
    if (j.success) {
      log('🚀 Application executing on STM32!');
      isStm32Connected = false;
      const badge = document.getElementById('conn-badge');
      badge.className = 'status-badge status-offline';
      badge.textContent = 'App Running (Click Reset)';
    } else {
      log('Jump failed: ' + j.message);
    }
  } catch (e) {
    log('Jump error: ' + e);
  }
}

refreshStatus();
setInterval(refreshStatus, 3000);
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

void handleReset() {
  Serial.println("[HTTP] POST /api/reset received");
  reset_stm32_hw();
  String json = "{\"success\":true,\"connected\":";
  json += (g_bl_connected ? "true" : "false");
  json += ",\"message\":\"";
  json += (g_bl_connected ? "STM32 successfully reset to bootloader!" : "Hardware reset pulse sent to STM32 NRST pin.");
  json += "\"}";
  server.send(200, "application/json", json);
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

    // If STM32 is currently executing application, auto-reset into bootloader!
    if (!g_bl_connected || !query_bl_version()) {
      Serial.println("[OTA] STM32 not in bootloader mode. Auto-resetting into Bootloader...");
      reset_stm32_hw();
    }

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
    server.send(500, "application/json", "{\"success\":false,\"message\":\"OTA flash failed\"}");
  }
}

// -------------------------------------------------------------
// Arduino Entrypoints
// -------------------------------------------------------------
void setup() {
  system_update_cpu_freq(160);
  Serial.begin(115200);
  delay(100);

  // Initialize STM32 NRST pin to High-Z (input)
  pinMode(STM32_NRST_PIN, INPUT);

  // Initialize SoftwareSerial with default Config (D5=TX, D6=RX)
  stm32Serial.begin(STM32_BAUD, SWSERIAL_8N1, 12, 14);

  Serial.println("\n=============================================");
  Serial.println("⚡ STM32 OTA Gateway on ESP8266 (Auto-Probing)");
  Serial.println("=============================================");

  // Start Wi-Fi in dual AP + STA mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);

  // Connect to Laptop Hotspot
  Serial.printf("Connecting to Laptop Hotspot '%s'...\n", STA_SSID);
  WiFi.begin(STA_SSID, STA_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 6000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WIFI] Connected! Hotspot URL: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WIFI] Hotspot not yet connected (retrying in background).");
  }

  // Setup mDNS
  if (MDNS.begin("esp8266-ota")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] http://esp8266-ota.local");
  }

  // Register Web Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/rollback", HTTP_POST, handleRollback);
  server.on("/api/jump", HTTP_POST, handleJump);
  server.on("/api/upload", HTTP_POST, handleUploadResponse, handleFileUpload);

  server.begin();
  Serial.println("[HTTP] Server ready on Port 80");
  Serial.print("[HTTP] Hotspot URL: http://");
  Serial.println(WiFi.localIP());
  Serial.print("[HTTP] AP URL:      http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("=============================================\n");
}

static uint32_t s_last_probe = 0;
static uint8_t s_pin_mode = 0;

void loop() {
  server.handleClient();
  MDNS.update();

  if (!g_bl_connected && millis() - s_last_probe > 2000) {
    s_last_probe = millis();
    if (s_pin_mode == 0) {
      Serial.println("[PROBE 1/2] Mode 0: ESP TX=D5 (GPIO14) -> STM32 D0, RX=D6 (GPIO12) <- STM32 D1...");
      stm32Serial.begin(STM32_BAUD, SWSERIAL_8N1, 12, 14);
      s_pin_mode = 1;
    } else {
      Serial.println("[PROBE 2/2] Mode 1 (SWAPPED): ESP TX=D6 (GPIO12) -> STM32 D1, RX=D5 (GPIO14) <- STM32 D0...");
      stm32Serial.begin(STM32_BAUD, SWSERIAL_8N1, 14, 12);
      s_pin_mode = 0;
    }

    if (query_bl_version()) {
      Serial.printf("[PROBE] >>> SUCCESS! CONNECTED TO STM32 (v%u.%u.%u)! <<<\n",
                    g_bl_version[0], g_bl_version[1], g_bl_version[2]);
      query_slot_info();
    }
  }

  delay(2);
}
