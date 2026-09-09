import serial
import time
import os
import sys
import struct

PORT = "COM6"
BAUD = 115200

# Bootloader Command Codes
BL_GET_VERSION   = 0x51
BL_FLASH_ERASE   = 0x52
BL_MEM_WRITE     = 0x53
BL_VERIFY_CRC    = 0x54
BL_JUMP_APP      = 0x55
BL_GET_SLOT_INFO = 0x56
BL_ROLLBACK      = 0x57
BL_ACTIVATE_SLOT = 0x58

# Status Codes
BL_ACK  = 0xA5
BL_NACK = 0x7F

SLOT1_BASE_ADDRESS       = 0x08010000  # Sector 4 start (64 KB)
SLOT2_BASE_ADDRESS       = 0x08020000  # Sector 5 start (128 KB)
APPLICATION_BASE_ADDRESS = SLOT1_BASE_ADDRESS

def calc_crc(data: bytes) -> int:
    """
    Calculates 32-bit CRC matching the STM32 hardware CRC calculation unit:
    - Polynomial: 0x04C11DB7
    - Initial value: 0xFFFFFFFF
    - Input: byte-by-byte promoted to 32-bit word (0x000000XX) into CRC->DR
    - Output: final CRC accumulator with no XOR inversion
    """
    crc = 0xFFFFFFFF
    poly = 0x04C11DB7
    for b in data:
        crc ^= b
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ poly) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc

def connect_serial(port=PORT, baud=BAUD, timeout=2):
    try:
        ser = serial.Serial(port, baud, timeout=timeout)
        time.sleep(0.1)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        print(f"[+] Connected to {port} at {baud} baud")
        return ser
    except Exception as e:
        print(f"[-] Error opening serial port {port}: {e}")
        sys.exit(1)

def get_version(ser):
    """
    Query bootloader version with CRC verification.
    Packet layout:
    Length (5) | BL_GET_VERSION (0x51) | CRC (4 bytes LE)
    """
    print("[*] Reading Bootloader Version...")
    payload = bytes([BL_GET_VERSION])
    crc = calc_crc(payload)
    packet = bytearray([len(payload) + 4])
    packet.extend(payload)
    packet.extend(struct.pack("<I", crc))

    ser.write(packet)
    response = ser.read(4)  # ACK (0xA5) + 3 bytes (major, minor, patch)
    if response and response[0] == BL_ACK:
        major, minor, patch = response[1], response[2], response[3]
        print(f"[+] Bootloader Version: {major}.{minor}.{patch}")
        return (major, minor, patch)
    elif response and response[0] == BL_NACK:
        print("[-] Bootloader returned NACK (CRC check failed or invalid command)")
        return None
    else:
        print(f"[-] No valid response from bootloader (received: {response.hex() if response else 'None'})")
        return None

def test_crc_verification(ser):
    """
    Test CRC verification on bootloader:
    1. Send corrupted packet with invalid CRC -> expect NACK (0x7F)
    2. Send valid packet with correct CRC -> expect ACK (0xA5)
    """
    print("\n--- Testing CRC Verification ---")
    payload = bytes([BL_GET_VERSION])

    # Test 1: Corrupted CRC
    bad_crc = 0xDEADBEEF
    packet_bad = bytearray([len(payload) + 4])
    packet_bad.extend(payload)
    packet_bad.extend(struct.pack("<I", bad_crc))

    print("[*] 1. Sending packet with deliberately invalid CRC (0xDEADBEEF)...")
    ser.write(packet_bad)
    resp_bad = ser.read(1)
    if resp_bad == b'\x7F':
        print("    [+] PASS: Bootloader rejected invalid CRC with NACK (0x7F)")
    else:
        print(f"    [-] FAIL: Bootloader did not reject invalid CRC (got {resp_bad.hex() if resp_bad else 'None'})")
        return False

    # Test 2: Valid CRC
    good_crc = calc_crc(payload)
    packet_good = bytearray([len(payload) + 4])
    packet_good.extend(payload)
    packet_good.extend(struct.pack("<I", good_crc))

    print(f"[*] 2. Sending packet with valid CRC (0x{good_crc:08X})...")
    ser.write(packet_good)
    resp_good = ser.read(4)
    if resp_good and resp_good[0] == BL_ACK:
        print("    [+] PASS: Bootloader accepted valid CRC with ACK (0xA5)")
    else:
        print(f"    [-] FAIL: Bootloader rejected valid CRC (got {resp_good.hex() if resp_good else 'None'})")
        return False

    print("[+] CRC Verification system is 100% operational!")
    return True

def format_version(ver_int):
    major = (ver_int >> 16) & 0xFF
    minor = (ver_int >> 8) & 0xFF
    patch = ver_int & 0xFF
    return f"{major}.{minor}.{patch}"

def parse_version(ver_str):
    parts = ver_str.strip().split(".")
    major = int(parts[0]) if len(parts) > 0 else 1
    minor = int(parts[1]) if len(parts) > 1 else 0
    patch = int(parts[2]) if len(parts) > 2 else 0
    return (major << 16) | (minor << 8) | patch

def get_slot_info(ser):
    """
    Query dual-slot metadata table from bootloader.
    Packet layout:
    Length (5) | BL_GET_SLOT_INFO (0x56) | CRC (4 bytes LE)
    """
    payload = bytes([BL_GET_SLOT_INFO])
    crc = calc_crc(payload)
    packet = bytearray([len(payload) + 4])
    packet.extend(payload)
    packet.extend(struct.pack("<I", crc))

    ser.write(packet)
    resp = ser.read(19)  # ACK (0xA5) + 18 bytes payload
    if not resp or resp[0] != BL_ACK or len(resp) < 19:
        print(f"[-] Failed to get slot info (response: {resp.hex() if resp else 'None'})")
        return None

    data = resp[1:]
    active_slot = data[0]
    prev_slot = data[1]
    s1_state = "VALID" if data[2] == 1 else "EMPTY"
    s2_state = "VALID" if data[3] == 1 else "EMPTY"
    s1_ver = struct.unpack("<I", data[4:8])[0]
    s2_ver = struct.unpack("<I", data[8:12])[0]
    update_cnt = struct.unpack("<I", data[12:16])[0]
    oldest_slot = data[16]
    rollback_ok = (data[17] == 1)

    info = {
        "active_slot": active_slot,
        "prev_slot": prev_slot,
        "slot1_state": s1_state,
        "slot2_state": s2_state,
        "slot1_version": format_version(s1_ver),
        "slot2_version": format_version(s2_ver),
        "update_counter": update_cnt,
        "oldest_slot": oldest_slot,
        "rollback_possible": rollback_ok
    }

    print("\n================ DUAL-SLOT STATUS ================")
    print(f" Active Slot        : Slot {active_slot} ({'Slot 1: 0x08010000' if active_slot == 1 else 'Slot 2: 0x08020000'})")
    print(f" Previous (Rollback): {'Slot ' + str(prev_slot) if prev_slot else 'None'}")
    print(f" Slot 1 (0x08010000): [{s1_state}] Version: {format_version(s1_ver)}")
    print(f" Slot 2 (0x08020000): [{s2_state}] Version: {format_version(s2_ver)}")
    print(f" Update Counter     : {update_cnt}")
    print(f" Oldest Slot (Next) : Slot {oldest_slot}")
    print(f" Rollback Available : {'YES' if rollback_ok else 'NO'}")
    print("==================================================")

    return info

def activate_slot(ser, target_slot, version_int):
    """
    Activate slot after flashing.
    Packet layout:
    Length (10) | BL_ACTIVATE_SLOT (0x58) | Slot (1B) | Version (4B LE) | CRC (4B LE)
    """
    payload = bytearray([BL_ACTIVATE_SLOT, target_slot])
    payload.extend(struct.pack("<I", version_int))
    crc = calc_crc(payload)

    packet = bytearray([len(payload) + 4])
    packet.extend(payload)
    packet.extend(struct.pack("<I", crc))

    ser.write(packet)
    resp = ser.read(2)  # ACK (0xA5) + Status (0x00)
    if resp and resp[0] == BL_ACK and resp[1] == 0x00:
        print(f"[+] Slot {target_slot} activated successfully with version {format_version(version_int)}")
        return True
    else:
        print(f"[-] Failed to activate slot {target_slot} (response: {resp.hex() if resp else 'None'})")
        return False

def rollback(ser):
    """
    Rollback to the previous firmware slot.
    Packet layout:
    Length (5) | BL_ROLLBACK (0x57) | CRC (4B LE)
    """
    print("\n--- Initiating Firmware Rollback ---")
    payload = bytes([BL_ROLLBACK])
    crc = calc_crc(payload)
    packet = bytearray([len(payload) + 4])
    packet.extend(payload)
    packet.extend(struct.pack("<I", crc))

    ser.write(packet)
    resp = ser.read(3)  # ACK (0xA5) + Status (0x00) + Restored Slot (1 or 2)
    if resp and resp[0] == BL_ACK and resp[1] == 0x00:
        restored_slot = resp[2]
        print(f"[+] Rollback Successful! Restored active slot to Slot {restored_slot}!")
        print(f"[+] The microcontroller is now executing the restored firmware.")
        return True
    elif resp and resp[0] == BL_ACK and resp[1] != 0x00:
        print("[-] Rollback Failed: No valid previous firmware available in the other slot.")
        return False
    else:
        print(f"[-] Rollback command failed (response: {resp.hex() if resp else 'None'})")
        return False

def flash_erase(ser, sector=4, num_sectors=1):
    """
    Erase flash sectors with CRC verification.
    Packet layout:
    Length (7) | BL_FLASH_ERASE (0x52) | Sector | NumSectors | CRC (4 bytes LE)
    """
    print(f"[*] Erasing Flash: Sector {sector}, count = {num_sectors}...")
    payload = bytes([BL_FLASH_ERASE, sector, num_sectors])
    crc = calc_crc(payload)

    packet = bytearray([len(payload) + 4])
    packet.extend(payload)
    packet.extend(struct.pack("<I", crc))

    ser.write(packet)
    response = ser.read(2)
    print(f"    Erase Response: {response.hex() if response else 'None'}")

    if response == b'\xA5\x00':
        print("[+] Flash Erase Successful")
        return True
    elif response and response[0] == BL_NACK:
        print("[-] Flash Erase Failed: Bootloader returned NACK (CRC mismatch)")
        return False
    else:
        print(f"[-] Flash Erase Failed (response: {response.hex() if response else 'None'})")
        return False

def mem_write(ser, address, data_bytes):
    """
    Write a chunk of data (1 to 128 bytes) to flash address with CRC verification.
    Packet layout:
    Length | BL_MEM_WRITE (0x53) | Address (4 bytes LE) | Data (N bytes) | CRC (4 bytes LE)
    """
    payload = bytearray([BL_MEM_WRITE])
    payload.extend(struct.pack("<I", address))
    payload.extend(data_bytes)

    crc = calc_crc(payload)

    packet = bytearray([len(payload) + 4])
    packet.extend(payload)
    packet.extend(struct.pack("<I", crc))

    ser.write(packet)
    response = ser.read(2)

    if response == b'\xA5\x00':
        return True
    elif response and response[0] == BL_NACK:
        print(f"[-] Mem Write NACK (CRC error) at 0x{address:08X}")
        return False
    else:
        print(f"[-] Mem Write failed at 0x{address:08X} (response: {response.hex() if response else 'None'})")
        return False

def test_single_word_write(ser, address=0x08010000, test_word=0x12345678):
    """
    Test writing a single 32-bit word to flash.
    """
    print(f"\n--- Testing Single Word Write ---")
    if not flash_erase(ser, sector=4, num_sectors=1):
        return False

    time.sleep(0.1)
    data_bytes = struct.pack("<I", test_word)
    print(f"[*] Writing 0x{test_word:08X} to 0x{address:08X}...")
    if mem_write(ser, address, data_bytes):
        print(f"[+] Memory Write Successful: 0x{test_word:08X} written to 0x{address:08X}")
        return True
    else:
        print(f"[-] Memory Write Failed")
        return False

def flash_firmware(ser, bin_path, base_address=APPLICATION_BASE_ADDRESS, chunk_size=64):
    """
    Erase application flash and write full binary firmware in chunks.
    """
    if not os.path.exists(bin_path):
        print(f"[-] Firmware file not found: {bin_path}")
        return False

    with open(bin_path, "rb") as f:
        firmware = f.read()

    total_size = len(firmware)
    print(f"\n--- Flashing Full Firmware ---")
    print(f"[*] Firmware file: {bin_path}")
    print(f"[*] Firmware size: {total_size} bytes")
    print(f"[*] Base address : 0x{base_address:08X}")
    print(f"[*] Chunk size   : {chunk_size} bytes")

    # Determine sectors to erase (Sector 4 is 64KB: 0x08010000 - 0x0801FFFF)
    sectors_to_erase = 1
    if total_size > 64 * 1024:
        sectors_to_erase = 2  # Sector 5 is 128KB

    if not flash_erase(ser, sector=4, num_sectors=sectors_to_erase):
        return False

    time.sleep(0.1)

    print("[*] Writing firmware packets...")
    bytes_written = 0
    start_time = time.time()

    while bytes_written < total_size:
        chunk = firmware[bytes_written : bytes_written + chunk_size]
        curr_addr = base_address + bytes_written

        if not mem_write(ser, curr_addr, chunk):
            print(f"\n[-] Failed to flash firmware at offset {bytes_written} (addr: 0x{curr_addr:08X})")
            return False

        bytes_written += len(chunk)
        percent = (bytes_written / total_size) * 100
        print(f"\r    Progress: {bytes_written}/{total_size} bytes ({percent:.1f}%)", end="", flush=True)

    elapsed = time.time() - start_time
    print(f"\n[+] Firmware flashed successfully in {elapsed:.2f}s ({total_size / elapsed:.1f} bytes/s)")
    return True

def jump_to_app(ser):
    """
    Command bootloader to jump to application with CRC verification.
    Packet layout:
    Length (5) | BL_JUMP_APP (0x55) | CRC (4 bytes LE)
    """
    print(f"\n--- Jumping to Application ---")
    payload = bytes([BL_JUMP_APP])
    crc = calc_crc(payload)

    packet = bytearray([len(payload) + 4])
    packet.extend(payload)
    packet.extend(struct.pack("<I", crc))

    ser.write(packet)
    response = ser.read(2)
    print(f"    Jump Response: {response.hex() if response else 'None'}")

    if response == b'\xA5\x00':
        print("[+] Application jumped successfully! The application is now executing.")
        return True
    elif response and response[0] == BL_NACK:
        print("[-] Jump to application failed: Bootloader returned NACK (CRC mismatch)")
        return False
    else:
        print(f"[-] Jump to application failed (response: {response.hex() if response else 'None'})")
        return False

def update_firmware(ser, bin_path=None, version_str=None):
    """
    Automatic dual-slot firmware update:
    1. Query slot info to determine the oldest slot.
    2. Erase the target slot's flash sector.
    3. Program the new firmware in chunks with CRC per packet.
    4. Activate the newly flashed slot with the version tag.
    5. Jump to the newly activated firmware.
    """
    info = get_slot_info(ser)
    if not info:
        return False

    target_slot = info["oldest_slot"]
    target_addr = SLOT1_BASE_ADDRESS if target_slot == 1 else SLOT2_BASE_ADDRESS
    target_sector = 4 if target_slot == 1 else 5

    script_dir = os.path.dirname(os.path.abspath(__file__))
    debug_dir = os.path.join(script_dir, "application", "Debug")

    if not bin_path:
        if target_slot == 2:
            bin_path = os.path.join(debug_dir, "application_v2.bin")
            version_str = version_str or "2.0.0"
        else:
            bin_path = os.path.join(debug_dir, "application_v3.bin")
            version_str = version_str or "3.0.0"

    print(f"\n[*] Target for Update: Slot {target_slot} (Sector {target_sector} @ 0x{target_addr:08X}) [Replacing Oldest]")

    if not os.path.exists(bin_path):
        print(f"[-] Firmware file not found: {bin_path}")
        return False

    with open(bin_path, "rb") as f:
        firmware = f.read()

    total_size = len(firmware)
    print(f"[*] Firmware File    : {bin_path}")
    print(f"[*] Firmware Size    : {total_size} bytes")
    print(f"[*] Firmware Version : {version_str or '2.0.0'}")

    # Erase target sector
    if not flash_erase(ser, sector=target_sector, num_sectors=1):
        return False

    time.sleep(0.1)

    # Flash in chunks
    print("[*] Writing firmware packets with CRC...")
    bytes_written = 0
    chunk_size = 64
    start_time = time.time()

    while bytes_written < total_size:
        chunk = firmware[bytes_written : bytes_written + chunk_size]
        curr_addr = target_addr + bytes_written

        if not mem_write(ser, curr_addr, chunk):
            print(f"\n[-] Failed to flash firmware at offset {bytes_written}")
            return False

        bytes_written += len(chunk)
        percent = (bytes_written / total_size) * 100
        print(f"\r    Progress: {bytes_written}/{total_size} bytes ({percent:.1f}%)", end="", flush=True)

    elapsed = time.time() - start_time
    print(f"\n[+] Firmware flashed successfully in {elapsed:.2f}s ({total_size / elapsed:.1f} bytes/s)")

    # Compute version integer
    ver_val = parse_version(version_str) if version_str else 0x00020000
    if not activate_slot(ser, target_slot, ver_val):
        return False

    time.sleep(0.1)

    # Jump to newly activated slot
    return jump_to_app(ser)

def main():
    ser = connect_serial()

    try:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        app_bin = os.path.join(script_dir, "application", "Debug", "application_v1.bin")
        if not os.path.exists(app_bin):
            app_bin = os.path.join(script_dir, "application", "Debug", "application.bin")

        if len(sys.argv) > 1 and sys.argv[1] == "--version":
            get_version(ser)
        elif len(sys.argv) > 1 and sys.argv[1] == "--slot-info":
            get_slot_info(ser)
        elif len(sys.argv) > 1 and sys.argv[1] == "--update":
            bin_arg = sys.argv[2] if len(sys.argv) > 2 else None
            ver_arg = sys.argv[3] if len(sys.argv) > 3 else None
            update_firmware(ser, bin_path=bin_arg, version_str=ver_arg)
        elif len(sys.argv) > 1 and sys.argv[1] == "--rollback":
            rollback(ser)
        elif len(sys.argv) > 1 and sys.argv[1] == "--test-crc":
            test_crc_verification(ser)
        elif len(sys.argv) > 1 and sys.argv[1] == "--test-word":
            test_single_word_write(ser)
        elif len(sys.argv) > 1 and sys.argv[1] == "--jump":
            jump_to_app(ser)
        elif len(sys.argv) > 1 and sys.argv[1] == "--flash":
            flash_firmware(ser, app_bin)
        else:
            # Full flow: Version check -> Slot Info -> Flash Application Slot 1 -> Jump
            print("==================================================")
            print("Step 1: Check Bootloader Version & CRC Link")
            print("==================================================")
            ver = get_version(ser)
            if not ver:
                print("[-] Could not retrieve version, aborting.")
                return

            print("\n==================================================")
            print("Step 2: Inspect Dual-Slot Metadata")
            print("==================================================")
            get_slot_info(ser)

            time.sleep(0.2)

            print("\n==================================================")
            print("Step 3: Flash Slot 1 Firmware (v1.0.0, 200 ms LED)")
            print("==================================================")
            if not flash_firmware(ser, app_bin, base_address=SLOT1_BASE_ADDRESS, chunk_size=64):
                print("[-] Step 3 failed, aborting.")
                return
            activate_slot(ser, 1, parse_version("1.0.0"))

            time.sleep(0.2)

            print("\n==================================================")
            print("Step 4: Jump to Active Application")
            print("==================================================")
            jump_to_app(ser)

    finally:
        ser.close()
        print("\n[*] Serial connection closed.")

if __name__ == "__main__":
    main()