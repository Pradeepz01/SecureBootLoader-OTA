import os
import subprocess
import sys

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

SOURCES = [
    os.path.join(APP_DIR, "Src", "main.c"),
    os.path.join(APP_DIR, "Src", "syscalls.c"),
    os.path.join(APP_DIR, "Src", "sysmem.c"),
    os.path.join(APP_DIR, "Src", "systick.c"),
    os.path.join(APP_DIR, "Startup", "startup_stm32f446retx.s")
]

def build_slot_app(version_name, delay_ms, linker_script, output_bin):
    print(f"[*] Building {version_name} (delay={delay_ms}ms, linker={os.path.basename(linker_script)})...")
    objs = []
    
    # Compile sources
    for src in SOURCES:
        base = os.path.splitext(os.path.basename(src))[0]
        obj = os.path.join(DEBUG_DIR, f"{base}_{version_name}.o")
        objs.append(obj)
        
        extra_flags = [f"-DBLINK_DELAY_MS={delay_ms}"] if "main.c" in src else []
        cmd = [CC, src] + CFLAGS + extra_flags + ["-o", obj]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[-] Compilation error for {src}:\n{res.stderr}")
            return False

    # Link
    elf = os.path.join(DEBUG_DIR, f"{version_name}.elf")
    cmd_link = [CC, "-o", elf] + objs + [f"-T{linker_script}"] + LDFLAGS
    res = subprocess.run(cmd_link, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[-] Link error for {version_name}:\n{res.stderr}")
        return False

    # Convert to bin
    cmd_bin = [OBJCOPY, "-O", "binary", elf, output_bin]
    res = subprocess.run(cmd_bin, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[-] Objcopy error for {version_name}:\n{res.stderr}")
        return False

    size = os.path.getsize(output_bin)
    print(f"[+] Successfully built {output_bin} ({size} bytes)")
    return True

def main():
    os.makedirs(DEBUG_DIR, exist_ok=True)
    
    ld_slot1 = os.path.join(APP_DIR, "STM32F446RETX_FLASH.ld")
    ld_slot2 = os.path.join(APP_DIR, "STM32F446RETX_FLASH_SLOT2.ld")

    # 1. Version 1 (Slot 1, 0x08010000, 200 ms fast blink)
    v1_bin = os.path.join(DEBUG_DIR, "application_v1.bin")
    build_slot_app("app_v1", 200, ld_slot1, v1_bin)

    # 2. Version 2 (Slot 2, 0x08020000, 800 ms slow blink)
    v2_bin = os.path.join(DEBUG_DIR, "application_v2.bin")
    build_slot_app("app_v2", 800, ld_slot2, v2_bin)

    # 3. Version 3 (Slot 1, 0x08010000, 50 ms rapid blink)
    v3_bin = os.path.join(DEBUG_DIR, "application_v3.bin")
    build_slot_app("app_v3", 50, ld_slot1, v3_bin)

if __name__ == "__main__":
    main()
