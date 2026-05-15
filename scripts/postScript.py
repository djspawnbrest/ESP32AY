import os
import sys
Import("env")

# ANSI color codes
class Colors:
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    MAGENTA = '\033[95m'
    BOLD = '\033[1m'
    RESET = '\033[0m'

# Красивый вывод информации о сборке
def print_build_info(env):
    """Print beautiful build information"""
    project_dir = env["PROJECT_DIR"]
    build_info_file = os.path.join(project_dir, "src", "build_info.h")
    
    # Read build info
    build_number = "????"
    build_date = "??.??.??"
    full_version = "?.?.????"
    
    if os.path.exists(build_info_file):
        try:
            with open(build_info_file, 'r') as f:
                content = f.read()
                import re
                
                match = re.search(r'#define\s+BUILD_NUMBER\s+"([^"]+)"', content)
                if match:
                    build_number = match.group(1)
                
                match = re.search(r'#define\s+BUILD_DATE\s+"([^"]+)"', content)
                if match:
                    build_date = match.group(1)
                
                match = re.search(r'#define\s+FULL_VERSION\s+"([^"]+)"', content)
                if match:
                    full_version = match.group(1)
        except IOError:
            pass
    
    # Print beautiful box with colors
    env_name = env['PIOENV']
    print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.GREEN}  ✓ BUILD COMPLETE: {Colors.YELLOW}{env_name}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.RESET}")
    print(f"  {Colors.CYAN}Version:{Colors.RESET}      {Colors.BOLD}{Colors.GREEN}{full_version}{Colors.RESET}")
    print(f"  {Colors.CYAN}Build Number:{Colors.RESET} {Colors.BOLD}{Colors.YELLOW}{build_number}{Colors.RESET}")
    print(f"  {Colors.CYAN}Build Date:{Colors.RESET}   {Colors.BOLD}{Colors.MAGENTA}{build_date}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.RESET}\n")
    sys.stdout.flush()

# Путь к папке с прошивками
firmware_dir = os.path.join(env["PROJECT_DIR"], "firmware")
os.makedirs(firmware_dir, exist_ok=True)  # Создаём папку, если её нет

# Функция для объединения прошивок
def merge_bin(source, target, env):
    # Print build info first
    print_build_info(env)
    merged_bin_path = os.path.join(firmware_dir, f"{env['PIOENV']}.bin")
    
    # Пути файлов прошивки (ordered list for ESP32)
    flash_images = [
        ("0x1000", os.path.join(env["BUILD_DIR"], "bootloader.bin")),
        ("0x8000", os.path.join(env["BUILD_DIR"], "partitions.bin")),
        ("0xe000", os.path.join(env["PROJECT_CORE_DIR"], "packages/framework-arduinoespressif32/tools/partitions", "boot_app0.bin")),
        ("0x10000", os.path.join(env["BUILD_DIR"], "firmware.bin")),
    ]

    # Формируем команду для объединения прошивки
    command = " ".join([
        "$PYTHONEXE",
        "$OBJCOPY",
        "--chip esp32",
        "merge_bin",
        "-o", merged_bin_path,
        "--flash_mode keep",
        "--flash_freq keep",
        "--flash_size keep"
    ] + [f"{addr} {path}" for addr, path in flash_images])

    # Выполнение команды
    env.Execute(command)

# Добавляем действие после сборки для объединения прошивок
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
