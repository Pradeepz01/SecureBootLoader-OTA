#!/usr/bin/env python3
"""
ESP32 Wireless OTA Client CLI for STM32F446RE Secure Bootloader
Communicates with the ESP32 Gateway over Wi-Fi (REST API).

Usage Examples:
    # 1. Check Bootloader & Dual-Slot Status:
    python ota_client.py --ip 192.168.4.1 status

    # 2. Upload and Flash New Firmware (.bin) Wirelessly:
    python ota_client.py --ip 192.168.4.1 flash application.bin

    # 3. Trigger Instant A/B Slot Rollback:
    python ota_client.py --ip 192.168.4.1 rollback

    # 4. Jump to the Active Application:
    python ota_client.py --ip 192.168.4.1 jump
"""

import sys
import os
import argparse
import time
import urllib.request
import urllib.parse
import json

def format_version(v):
    if not v:
        return "Empty"
    major = (v >> 16) & 0xFF
    minor = (v >> 8) & 0xFF
    patch = v & 0xFF
    return f"v{major}.{minor}.{patch}"

def cmd_status(ip):
    url = f"http://{ip}/api/status"
    print(f"[*] Querying status from ESP32 Gateway at {url}...")
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'ESP32-OTA-Client'})
        with urllib.request.urlopen(req, timeout=5) as response:
            data = json.loads(response.read().decode())
            print("\n=======================================================")
            print("         STM32 SECURE BOOTLOADER STATUS OVER WI-FI     ")
            print("=======================================================")
            conn = data.get("connected", False)
            bl_ver = data.get("bl_version", [0, 0, 0])
            print(f"STM32 Bootloader Connection : {'ONLINE' if conn else 'OFFLINE / UNRESPONSIVE'}")
            print(f"Bootloader Protocol Version : v{bl_ver[0]}.{bl_ver[1]}.{bl_ver[2]}")
            
            slots = data.get("slots", {})
            if slots.get("valid", False):
                active = slots.get("active_slot", 0)
                prev = slots.get("prev_slot", 0)
                s1_ver = format_version(slots.get("slot1_version", 0))
                s2_ver = format_version(slots.get("slot2_version", 0))
                counter = slots.get("update_counter", 0)
                oldest = slots.get("oldest_slot", 0)
                rb_avail = bool(slots.get("rollback_possible", 0))

                print(f"\nActive Slot Execution Bank  : Slot {active} (0x{0x08010000 if active == 1 else 0x08020000:08X})")
                print(f"Previous Fallback Slot      : Slot {prev}")
                print(f"Slot 1 (Sector 4 - 64 KB)   : {s1_ver} [{'ACTIVE' if active == 1 else 'STANDBY'}]")
                print(f"Slot 2 (Sector 5 - 128 KB)  : {s2_ver} [{'ACTIVE' if active == 2 else 'STANDBY'}]")
                print(f"Monotonic Update Counter    : #{counter}")
                print(f"Target for Next OTA Update  : Slot {oldest} (Auto-Replacing Oldest)")
                print(f"Rollback Available          : {'YES (Ready)' if rb_avail else 'NO'}")
            else:
                print("[-] Slot Table: Uninitialized or unreadable.")
            print("=======================================================\n")
    except Exception as e:
        print(f"[-] Failed to connect to ESP32 at {ip}: {e}")

def cmd_flash(ip, bin_path):
    if not os.path.exists(bin_path):
        print(f"[-] Error: Firmware file not found: {bin_path}")
        return

    file_size = os.path.getsize(bin_path)
    filename = os.path.basename(bin_path)
    url = f"http://{ip}/api/upload"

    print(f"[*] Preparing OTA Firmware Stream:")
    print(f"    File      : {bin_path}")
    print(f"    Size      : {file_size} bytes")
    print(f"    Target IP : {ip}")

    # Multipart form-data encoding
    boundary = '----WebKitFormBoundary' + hex(int(time.time() * 1000))[2:]
    
    with open(bin_path, 'rb') as f:
        file_bytes = f.read()

    body = (
        f'--{boundary}\r\n'
        f'Content-Disposition: form-data; name="firmware"; filename="{filename}"\r\n'
        f'Content-Type: application/octet-stream\r\n\r\n'
    ).encode('utf-8') + file_bytes + f'\r\n--{boundary}--\r\n'.encode('utf-8')

    req = urllib.request.Request(url, data=body)
    req.add_header('Content-Type', f'multipart/form-data; boundary={boundary}')
    req.add_header('Content-Length', str(len(body)))

    print(f"[*] Streaming firmware over Wi-Fi to ESP32 Gateway...")
    t0 = time.time()
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            t1 = time.time()
            res_data = json.loads(resp.read().decode())
            if res_data.get("success", False):
                dt = t1 - t0
                tput = (file_size / dt) if dt > 0 else 0
                print(f"\n[+] WIRELESS OTA UPDATE SUCCESSFUL!")
                print(f"    Bytes Written : {res_data.get('bytes', file_size)} bytes")
                print(f"    Elapsed Time  : {dt:.2f} seconds")
                print(f"    Throughput    : {tput:.1f} B/s ({tput/1024.0:.2f} KB/s)")
                print(f"    Status        : Target slot erased, programmed with CRC, and activated.\n")
            else:
                print(f"[-] Flash rejected by bootloader: {res_data.get('message', 'Unknown error')}")
    except Exception as e:
        print(f"[-] Upload failed: {e}")

def cmd_rollback(ip):
    url = f"http://{ip}/api/rollback"
    print(f"[*] Requesting instantaneous A/B slot rollback from {url}...")
    t0 = time.time()
    try:
        req = urllib.request.Request(url, data=b'', method='POST')
        with urllib.request.urlopen(req, timeout=5) as resp:
            t1 = time.time()
            data = json.loads(resp.read().decode())
            if data.get("success", False):
                target = data.get("target_slot", 0)
                print(f"[+] ROLLBACK SUCCESSFUL in {(t1 - t0)*1000.0:.1f} ms!")
                print(f"    Active Slot switched to: Slot {target}")
                print(f"    Bootloader updated metadata and verified fallback integrity.\n")
            else:
                print(f"[-] Rollback rejected: {data.get('message', 'Unknown error')}")
    except Exception as e:
        print(f"[-] Rollback failed: {e}")

def cmd_jump(ip):
    url = f"http://{ip}/api/jump"
    print(f"[*] Requesting application start from {url}...")
    try:
        req = urllib.request.Request(url, data=b'', method='POST')
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read().decode())
            if data.get("success", False):
                print(f"[+] Application jumped successfully! Firmware is now executing on STM32.\n")
            else:
                print(f"[-] Jump rejected: {data.get('message', 'Unknown error')}")
    except Exception as e:
        print(f"[-] Jump request failed: {e}")

def main():
    parser = argparse.ArgumentParser(description="Wireless OTA Client for STM32 Secure Bootloader via ESP32")
    parser.add_argument("--ip", default="192.168.4.1", help="IP address of ESP32 Gateway (default: 192.168.4.1)")
    
    subparsers = parser.add_subparsers(dest="command", required=True)
    
    subparsers.add_parser("status", help="Query bootloader and dual-slot status")
    
    flash_parser = subparsers.add_parser("flash", help="Upload and flash binary firmware image")
    flash_parser.add_argument("file", help="Path to .bin firmware file")

    subparsers.add_parser("rollback", help="Trigger instantaneous A/B slot rollback")
    subparsers.add_parser("jump", help="Trigger jump to active application")

    args = parser.parse_args()

    if args.command == "status":
        cmd_status(args.ip)
    elif args.command == "flash":
        cmd_flash(args.ip, args.file)
    elif args.command == "rollback":
        cmd_rollback(args.ip)
    elif args.command == "jump":
        cmd_jump(args.ip)

if __name__ == "__main__":
    main()
