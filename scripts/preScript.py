Import("env")
import os
import re
import time
import sys
from datetime import datetime

# ANSI color codes
class Colors:
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BOLD = '\033[1m'
    RESET = '\033[0m'

def increment_build_number(env):
    """Increment build number and generate build_info.h"""
    project_dir = env["PROJECT_DIR"]
    build_number_file = os.path.join(project_dir, "build_number.txt")
    session_file = os.path.join(project_dir, ".build_session")
    version_file = os.path.join(project_dir, ".build_version")
    build_info_file = os.path.join(project_dir, "src", "build_info.h")
    defines_file = os.path.join(project_dir, "src", "defines.h")
    
    # Read FW_VERSION from defines.h
    fw_version = "3.7"  # default
    try:
        with open(defines_file, 'r') as f:
            content = f.read()
            match = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', content)
            if match:
                fw_version = match.group(1)
    except IOError:
        print(f"{Colors.RED}Warning: Could not read {defines_file}, using default version{Colors.RESET}")
    
    # Check if version changed - reset build number if it did
    version_changed = False
    if os.path.exists(version_file):
        try:
            with open(version_file, 'r') as f:
                old_version = f.read().strip()
                if old_version != fw_version:
                    version_changed = True
                    print(f"{Colors.YELLOW}Version changed from {old_version} to {fw_version} - resetting build number!{Colors.RESET}")
        except IOError:
            pass
    
    # Check if this is a buildfs target (skip increment for buildfs)
    targets = [str(t) for t in BUILD_TARGETS]
    is_buildfs = "buildfs" in targets or any("littlefs" in t for t in targets)
    
    if is_buildfs:
        # Just regenerate build_info.h without incrementing
        if os.path.exists(build_number_file):
            with open(build_number_file, 'r') as f:
                build_num = int(f.read().strip())
        else:
            build_num = 0
        
        print(f"{Colors.CYAN}[{env['PIOENV']}]{Colors.RESET} Buildfs target - using existing build number: {Colors.YELLOW}{build_num:04d}{Colors.RESET}")
    else:
        # Check session file with timestamp
        current_time = time.time()
        should_increment = True
        build_num = None
        
        if not version_changed and os.path.exists(session_file):
            try:
                with open(session_file, 'r') as f:
                    session_time = float(f.read().strip())
                    # If less than 180 seconds (3 minutes) passed - same build session
                    if current_time - session_time < 180:
                        should_increment = False
                        if os.path.exists(build_number_file):
                            with open(build_number_file, 'r') as f:
                                build_num = int(f.read().strip())
                        print(f"{Colors.CYAN}[{env['PIOENV']}]{Colors.RESET} Using existing build number: {Colors.YELLOW}{build_num:04d}{Colors.RESET}")
            except (ValueError, IOError):
                pass
        
        # Increment build number if needed
        if should_increment:
            if version_changed:
                # Reset to 0 when version changes
                build_num = 0
            elif os.path.exists(build_number_file):
                with open(build_number_file, 'r') as f:
                    build_num = int(f.read().strip())
            else:
                build_num = 0
            
            build_num += 1
            
            # Write new build number
            with open(build_number_file, 'w') as f:
                f.write(f"{build_num:04d}")
            
            # Write current version
            with open(version_file, 'w') as f:
                f.write(fw_version)
            
            # Write session timestamp
            with open(session_file, 'w') as f:
                f.write(str(current_time))
            
            print(f"{Colors.CYAN}[{env['PIOENV']}]{Colors.RESET} {Colors.GREEN}Build number incremented to: {Colors.BOLD}{Colors.YELLOW}{build_num:04d}{Colors.RESET}")
    
    # Generate build date in DD.MM.YY format
    build_date = datetime.now().strftime("%d.%m.%y")
    
    # Generate full version
    full_version = f"{fw_version}.{build_num:04d}"
    
    # Generate build_info.h
    build_info_content = f"""#ifndef BUILD_INFO_H
#define BUILD_INFO_H

#define BUILD_NUMBER "{build_num:04d}"
#define BUILD_DATE "{build_date}"
#define FULL_VERSION "{full_version}"

#endif
"""
    
    with open(build_info_file, 'w') as f:
        f.write(build_info_content)
    
    print(f"{Colors.CYAN}[{env['PIOENV']}]{Colors.RESET} Generated build_info.h: Version {Colors.GREEN}{full_version}{Colors.RESET}, Date {Colors.GREEN}{build_date}{Colors.RESET}")
    sys.stdout.flush()

# Run build number increment before compilation
increment_build_number(env)

# Добавление действия после сборки для выполнения команды pio run -t buildfs -e esp32
env.AddPreAction("$BUILD_DIR/${PROGNAME}.bin", f"pio run -t buildfs -e {env['PIOENV']}")
