import os
Import("env")

# Directory for firmware files
firmware_dir = os.path.join(env["PROJECT_DIR"], "firmware")
os.makedirs(firmware_dir, exist_ok=True)  # Create directory if it doesn't exist

# Function to merge firmware files
def merge_bin(source, target, env):
    merged_bin_path = os.path.join(firmware_dir, f"{env['PIOENV']}.bin")
    
    # Resolve firmware file paths (ordered list for ESP32-S3)
    flash_images = [
        ("0x0", os.path.join(env["BUILD_DIR"], "bootloader.bin")),
        ("0x8000", os.path.join(env["BUILD_DIR"], "partitions.bin")),
        ("0xe000", os.path.join(env["PROJECT_CORE_DIR"], "packages/framework-arduinoespressif32/tools/partitions", "boot_app0.bin")),
        ("0x10000", os.path.join(env["BUILD_DIR"], "firmware.bin")),
    ]

    # Create command for merging firmware - updated for ESP32-S3
    command = " ".join([
        "$PYTHONEXE",
        "$OBJCOPY",
        "--chip esp32s3",
        "merge_bin",
        "-o", merged_bin_path,
        "--flash_mode keep",
        "--flash_freq keep",
        "--flash_size keep"
    ] + [f"{addr} {path}" for addr, path in flash_images])

    # Execute command
    env.Execute(command)

# Add post-build action to merge firmware files
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
