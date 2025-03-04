import shutil
import os
Import("env")

# Функция для копирования файлов прошивки
def firmware_action(source, target, env):
    src = target[0].get_abspath()
    filename = os.path.basename(src).split('/')[-1]
    dest = os.path.join(env["PROJECT_DIR"], "firmware", filename)
    shutil.copy(src, dest)

# Добавление действий после сборки для копирования файлов
env.AddPostAction("$BUILD_DIR/bootloader.bin", firmware_action)
env.AddPostAction("$BUILD_DIR/partitions.bin", firmware_action)
# env.AddPostAction("$BUILD_DIR/littlefs.bin", firmware_action)
env.AddPostAction("$BUILD_DIR/firmware.bin", firmware_action)

# Копирование boot_app0.bin
src_boot = os.path.join(env["PROJECT_CORE_DIR"], "packages/framework-arduinoespressif32/tools/partitions", "boot_app0.bin")
dest_boot = os.path.join(env["PROJECT_DIR"], "firmware", "boot_app0.bin")
shutil.copy(src_boot, dest_boot)

# Путь к папке с прошивками
firmware_dir = os.path.join(env['PROJECT_DIR'], 'firmware')

# Компоненты прошивки с адресами
flash_images = [
    "0x1000 bootloader.bin",
    "0x8000 partitions.bin",
    "0xe000 boot_app0.bin",
    "0x10000 firmware.bin",
    # "0x290000 littlefs.bin"
]

# Полные пути к файлам прошивки
flash_images_paths = [os.path.join(firmware_dir, img.split()[1]) for img in flash_images]

# Функция для объединения прошивок
def merge_bin(source, target, env):
    # Путь к объединенному файлу прошивки
    merged_bin_path = os.path.join(firmware_dir, "merged_firmware.bin")

    # Команда для объединения прошивок
    command = " ".join([
        "$PYTHONEXE",
        "$OBJCOPY",
        "--chip esp32",
        "merge_bin",
        "-o", merged_bin_path,
        "--flash_mode qio",
        "--flash_freq 80m",
        "--flash_size 4MB"
    ] + [f"{img.split()[0]} {path}" for img, path in zip(flash_images, flash_images_paths)])

    # Выполнение команды
    env.Execute(command)

# Добавление действия после сборки для объединения прошивок
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)