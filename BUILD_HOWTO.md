# ESP-Miner Build Guide

Complete guide for building and flashing ESP-Miner firmware for Bitaxe hardware.

## Prerequisites

### Required Software
- macOS (tested on current version)
- Homebrew
- Python 3.12 or 3.13 (via pyenv)
- ESP-IDF v5.3
- Git

### Hardware
- ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
- USB-C cable (data capable)
- Bitaxe board

## Initial Setup (One-Time)

### 1. Install System Dependencies

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required packages
brew install cmake ninja python3 git wget
```

### 2. Install pyenv and Python

```bash
# Install pyenv
brew install pyenv

# Install Python 3.13
pyenv install 3.13.3
pyenv global 3.13.3

# Add to shell profile (if not already added)
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.bash_profile
echo 'command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.bash_profile
echo 'eval "$(pyenv init -)"' >> ~/.bash_profile
```

### 3. Fix SSL Certificate Issues

```bash
# Install certifi and set environment variables
pip3 install --upgrade certifi

# Add to ~/.bash_profile
echo 'export SSL_CERT_FILE=$(python3 -m certifi)' >> ~/.bash_profile
echo 'export REQUESTS_CA_BUNDLE=$(python3 -m certifi)' >> ~/.bash_profile

# Reload shell
source ~/.bash_profile
```

### 4. Install ESP-IDF v5.3

```bash
# Create ESP directory
mkdir -p ~/esp
cd ~/esp

# Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Checkout v5.3 release
git checkout -b release/v5.3 origin/release/v5.3
git submodule update --init --recursive

# Install ESP-IDF tools
./install.sh esp32,esp32s3

# Add IDF_PATH to shell profile
echo 'export IDF_PATH=${HOME}/esp/esp-idf' >> ~/.bash_profile
```

**IMPORTANT**: Do NOT add `source ${HOME}/esp/esp-idf/export.sh` to your `.bash_profile`. Source it manually when needed.

### 5. Clone ESP-Miner

```bash
cd ~/Work  # or your preferred location
git clone https://github.com/skot/ESP-Miner.git
cd ESP-Miner
```

## Building the Firmware

### 1. Set Up Build Environment

```bash
# Navigate to ESP-IDF
cd ~/esp/esp-idf

# Source the environment (do this in every new terminal)
. ./export.sh

# Navigate to ESP-Miner
cd ~/Work/ESP-Miner  # adjust path as needed
```

### 2. Apply Required Fixes

#### Fix A: Update Component Dependencies

Edit `components/stratum/CMakeLists.txt` and `main/CMakeLists.txt`:

Change:
```cmake
REQUIRES
    "json"
```

To:
```cmake
REQUIRES
    "cjson"
```

#### Fix B: Fix voltage_monitor.c

Edit `components/asic/voltage_monitor.c`:

Add after existing includes (around line 10):
```c
#include <string.h>
```

Change format specifier (around line 342):
```c
// From:
ESP_LOGI(TAG, "Scan interval set to %d ms", interval_ms);

// To:
ESP_LOGI(TAG, "Scan interval set to %"PRIu32" ms", interval_ms);
```

#### Fix C: Fix NULL pointer in ASIC initialization

Edit `components/asic/asic.c` (around line 20):

Change:
```c
ESP_LOGI(TAG, "Initializing %s", GLOBAL_STATE->DEVICE_CONFIG.family.asic.name);
```

To:
```c
const char* asic_name = GLOBAL_STATE->DEVICE_CONFIG.family.asic.name ? 
                        GLOBAL_STATE->DEVICE_CONFIG.family.asic.name : "Unknown ASIC";
ESP_LOGI(TAG, "Initializing %s", asic_name);
```

### 3. Configure menuconfig (if needed)

```bash
idf.py menuconfig
```

Verify these settings:
- **Component config** → **ESP PSRAM** → **Type of SPIRAM chip**: ESP-PSRAM64
- **Component config** → **ESP PSRAM** → **Mode**: Octal Mode
- **Serial flasher config** → **Flash size**: 16MB

### 4. Build the Firmware

```bash
# Clean previous builds
idf.py fullclean

# Build with specific config (for Bitaxe 401/Supra)
idf.py -D SDKCONFIG_DEFAULTS=config-401.cvs build

# Create merged binary
idf.py merge-bin
```

The merged binary will be at: `build/esp-miner-merged.bin`

## Flashing the Firmware

### 1. Find the Correct Serial Port

```bash
# List all serial ports
ls -la /dev/cu.*

# Look for USB serial port, typically:
# /dev/cu.usbmodem141201  (USB-JTAG mode)
# or similar
```

### 2. Create Configuration File

Create `config.cvs` with your settings:

```csv
key,type,encoding,value
main,namespace,,
hostname,data,string,yourMinerName
wifissid,data,string,YourWiFiSSID
wifipass,data,string,YourWiFiPassword
stratumurl,data,string,public-pool.io
stratumport,data,u16,21496
stratumuser,data,string,youruser.bitaxe
stratumpass,data,string,x
stratumdiff,data,u16,1000
asicfrequency,data,u16,485
asicvoltage,data,u16,1200
boardversion,data,string,700
devicemodel,data,string,supra
asicmodel,data,string,BM1368
```

### 3. Flash Using bitaxetool

```bash
# Install bitaxetool if not already installed
pip3 install bitaxetool

# Flash firmware and config
bitaxetool --port /dev/cu.usbmodem141201 \
           --config ./config.cvs \
           --firmware ./build/esp-miner-merged.bin
```

### 4. Monitor Serial Output

```bash
# Using idf.py
idf.py -p /dev/cu.usbmodem141201 monitor

# Or using screen
screen /dev/cu.usbmodem141201 115200
# Exit screen: Ctrl+A then K
```

## Troubleshooting

### Board Stuck in Download Mode

If you see "waiting for download" in the serial monitor:

```bash
# Power cycle the board
# Unplug USB, wait 5 seconds, plug back in
```

### Flash Fails with Timeout/Checksum Errors

1. Try a different USB cable
2. Try a different USB port (prefer USB 2.0 ports)
3. Lower baud rate:

```bash
esptool.py --chip esp32s3 \
  --port /dev/cu.usbmodem141201 \
  --baud 115200 \
  write_flash --flash_size 16MB 0x0 build/esp-miner-merged.bin
```

### Build Fails with Missing Components

```bash
# Clean everything and rebuild
rm -rf build managed_components dependencies.lock
idf.py fullclean
idf.py build
```

### Wrong Python Environment

If you see errors about wrong Python version:

```bash
# Remove old environments
rm -rf ~/.espressif/python_env/

# Reinstall ESP-IDF
cd ~/esp/esp-idf
./install.sh esp32,esp32s3

# Re-source
. ./export.sh
```

## Post-Flash

1. The device will reboot and connect to WiFi
2. Check serial monitor for IP address
3. Access web interface at the displayed IP
4. Configure mining pool and settings via web UI

## Quick Reference

### Common Commands

```bash
# Set up environment (every new terminal)
cd ~/esp/esp-idf && . ./export.sh

# Build
cd ~/Work/ESP-Miner
idf.py fullclean
idf.py build
idf.py merge-bin

# Flash
bitaxetool --port /dev/cu.usbmodem141201 \
           --config ./config.cvs \
           --firmware ./build/esp-miner-merged.bin

# Monitor
idf.py -p /dev/cu.usbmodem141201 monitor
```

### File Locations

- ESP-IDF: `~/esp/esp-idf`
- ESP-Miner Source: `~/Work/ESP-Miner` (or your path)
- Built Binary: `~/Work/ESP-Miner/build/esp-miner-merged.bin`
- Python Environments: `~/.espressif/python_env/`

## Notes

- Always use ESP-IDF v5.3 for this project
- The merged binary is approximately 15MB
- First boot may take longer due to NVS initialization
- PSRAM detection should show "Found 8MB PSRAM device"
- If thermal sensor fails, device will still work but fan runs at fixed speed
