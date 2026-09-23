import os
import subprocess
import sys
import shutil

TOOLCHAIN_BIN = r"D:\USEME FOLDER\STMIDESTORAGEHERE\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin"
CC = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-gcc.exe")
OBJCOPY = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-objcopy.exe")

APP_DIR = os.path.dirname(os.path.abspath(__file__))
DEBUG_DIR = os.path.join(APP_DIR, "Debug")

CFLAGS = [
    "-mcpu=cortex-m4",
    "-std=gnu11",
    "-g3",
    "-DDEBUG",
    "-DSTM32",
    "-DSTM32F4",
    "-DSTM32F446RETx",
    "-DNUCLEO_F446RE",
    "-c",
    f"-I{os.path.join(APP_DIR, 'Inc')}",
    "-O0",
    "-ffunction-sections",
    "-fdata-sections",
    "-Wall",
    "--specs=nano.specs",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-mthumb"
]

LDFLAGS = [
    "-mcpu=cortex-m4",
    "--specs=nosys.specs",
    "-Wl,--gc-sections",
    "-static",
    "--specs=nano.specs",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-mthumb",
    "-Wl,--start-group",
    "-lc",
    "-lm",
    "-Wl,--end-group"
]

COMMON_SOURCES = [
    os.path.join(APP_DIR, "Src", "syscalls.c"),
    os.path.join(APP_DIR, "Src", "sysmem.c"),
    os.path.join(APP_DIR, "Src", "systick.c"),
    os.path.join(APP_DIR, "Startup", "startup_stm32f446retx.s")
]

def build_app(app_name, main_src, linker_script, output_bin, extra_defines=None):
    print(f"[*] Building {app_name} (main={os.path.basename(main_src)}, linker={os.path.basename(linker_script)})...")
    objs = []
    sources = [main_src] + COMMON_SOURCES
    
    # Compile sources
    for src in sources:
        base = os.path.splitext(os.path.basename(src))[0]
        obj = os.path.join(DEBUG_DIR, f"{base}_{app_name}.o")
        objs.append(obj)
        
        extra_flags = extra_defines if (extra_defines and src == main_src) else []
        cmd = [CC, src] + CFLAGS + extra_flags + ["-o", obj]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[-] Compilation error for {src}:\n{res.stderr}")
            return False

    # Link
    elf = os.path.join(DEBUG_DIR, f"{app_name}.elf")
    cmd_link = [CC, "-o", elf] + objs + [f"-T{linker_script}"] + LDFLAGS
    res = subprocess.run(cmd_link, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[-] Link error for {app_name}:\n{res.stderr}")
        return False

    # Convert to bin
    cmd_bin = [OBJCOPY, "-O", "binary", elf, output_bin]
    res = subprocess.run(cmd_bin, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[-] Objcopy error for {app_name}:\n{res.stderr}")
        return False

    size = os.path.getsize(output_bin)
    print(f"[+] Successfully built {output_bin} ({size} bytes)")
    return True

def main():
    os.makedirs(DEBUG_DIR, exist_ok=True)
    
    ld_slot1 = os.path.join(APP_DIR, "STM32F446RETX_FLASH.ld")
    ld_slot2 = os.path.join(APP_DIR, "STM32F446RETX_FLASH_SLOT2.ld")

    src_blink = os.path.join(APP_DIR, "Src", "main_blink.c")
    src_button = os.path.join(APP_DIR, "Src", "main_button.c")
    src_legacy = os.path.join(APP_DIR, "Src", "main.c")

    # 1. Application 1: Blinking LED Program (250 ms toggle)
    blink_s1 = os.path.join(DEBUG_DIR, "app_blinking_led_slot1.bin")
    blink_s2 = os.path.join(DEBUG_DIR, "app_blinking_led_slot2.bin")
    build_app("app_blink_slot1", src_blink, ld_slot1, blink_s1)
    build_app("app_blink_slot2", src_blink, ld_slot2, blink_s2)

    # 2. Application 2: Push-Button Controlled LED Program (PC13 button -> PA5 LED)
    button_s1 = os.path.join(DEBUG_DIR, "app_button_led_slot1.bin")
    button_s2 = os.path.join(DEBUG_DIR, "app_button_led_slot2.bin")
    build_app("app_button_slot1", src_button, ld_slot1, button_s1)
    build_app("app_button_slot2", src_button, ld_slot2, button_s2)

    # 3. Legacy benchmark versions (v1, v2, v3)
    build_app("app_v1", src_legacy, ld_slot1, os.path.join(DEBUG_DIR, "application_v1.bin"), ["-DBLINK_DELAY_MS=200"])
    build_app("app_v2", src_legacy, ld_slot2, os.path.join(DEBUG_DIR, "application_v2.bin"), ["-DBLINK_DELAY_MS=800"])
    build_app("app_v3", src_legacy, ld_slot1, os.path.join(DEBUG_DIR, "application_v3.bin"), ["-DBLINK_DELAY_MS=50"])

    # Copy deliverables to distribution directories
    repo_root = os.path.abspath(os.path.join(APP_DIR, "..", "FIle for github", "SecureBootLoader-OTA"))
    dest_test_apps = os.path.join(repo_root, "test_applications")
    dest_fw_bins = os.path.join(repo_root, "firmware_binaries")
    os.makedirs(dest_test_apps, exist_ok=True)
    os.makedirs(dest_fw_bins, exist_ok=True)

    artifacts = [
        ("app_blinking_led_slot1.bin", blink_s1),
        ("app_blinking_led_slot2.bin", blink_s2),
        ("app_button_led_slot1.bin", button_s1),
        ("app_button_led_slot2.bin", button_s2),
        ("application_v1.bin", os.path.join(DEBUG_DIR, "application_v1.bin")),
        ("application_v2.bin", os.path.join(DEBUG_DIR, "application_v2.bin")),
        ("application_v3.bin", os.path.join(DEBUG_DIR, "application_v3.bin"))
    ]

    for fname, src_path in artifacts:
        if os.path.exists(src_path):
            shutil.copy2(src_path, os.path.join(dest_test_apps, fname))
            shutil.copy2(src_path, os.path.join(dest_fw_bins, fname))
            print(f"[+] Synced {fname} -> {dest_test_apps} and {dest_fw_bins}")

    # Also provide convenience default copies for Slot 1
    shutil.copy2(blink_s1, os.path.join(dest_test_apps, "app_blinking_led.bin"))
    shutil.copy2(button_s1, os.path.join(dest_test_apps, "app_button_led.bin"))
    shutil.copy2(blink_s1, os.path.join(dest_fw_bins, "app_blinking_led.bin"))
    shutil.copy2(button_s1, os.path.join(dest_fw_bins, "app_button_led.bin"))
    print("[+] Created convenience copies: app_blinking_led.bin & app_button_led.bin")

if __name__ == "__main__":
    main()
