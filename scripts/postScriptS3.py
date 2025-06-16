import os
Import("env")

# Directory for firmware files
firmware_dir = os.path.join(env["PROJECT_DIR"], "firmware")
os.makedirs(firmware_dir, exist_ok=True)  # Create directory if it doesn't exist

# Original firmware file paths
flash_images_paths = {
    "0x0": os.path.join(env["BUILD_DIR"], "bootloader.bin"),
    "0x8000": os.path.join(env["BUILD_DIR"], "partitions.bin"),
    "0xe000": os.path.join(env["PROJECT_CORE_DIR"], "packages/framework-arduinoespressif32/tools/partitions", "boot_app0.bin"),
    "0x10000": os.path.join(env["BUILD_DIR"], "firmware.bin"),
}

# Function to merge firmware files
def merge_bin(source, target, env):
    merged_bin_path = os.path.join(firmware_dir, f"{env['PIOENV']}.bin")

    # Create command for merging firmware - updated for ESP32-S3
    command = " ".join([
        "$PYTHONEXE",
        "$OBJCOPY",
        "--chip esp32s3",  # Changed from esp32 to esp32s3
        "merge_bin",
        "-o", merged_bin_path,
        "--flash_mode qio",  # Explicitly set to qio as per platformio.ini
        "--flash_freq 80m",
        "--flash_size 16MB",  # Set to 16MB as per platformio.ini
        "--target-offset 0x0"  # Added target offset for ESP32-S3
    ] + [f"{addr} {path}" for addr, path in flash_images_paths.items()])

    # Execute command
    env.Execute(command)

# Add post-build action to merge firmware files
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
