Import("env")
import os

# Добавление действия после сборки для выполнения команды pio run -t buildfs -e esp32
env.AddPreAction("$BUILD_DIR/${PROGNAME}.bin", f"pio run -t buildfs -e {env['PIOENV']}")


