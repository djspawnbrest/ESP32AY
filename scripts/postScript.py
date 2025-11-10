import os
Import("env")

# Путь к папке с прошивками
firmware_dir = os.path.join(env["PROJECT_DIR"], "firmware")
os.makedirs(firmware_dir, exist_ok=True)  # Создаём папку, если её нет

# Функция для объединения прошивок
def merge_bin(source, target, env):
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
