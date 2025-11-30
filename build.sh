#!/bin/bash
#
# ESP-Miner Build Script
# Builds ESP-Miner firmware for Bitaxe hardware
#
# Usage: ./build.sh [options]
#   Options:
#     -c, --clean       Full clean before build
#     -f, --flash       Flash after successful build
#     -m, --monitor     Open serial monitor after flash
#     -p, --port PORT   Serial port (default: auto-detect)
#     --config FILE     Config file path (default: ./config.cvs)
#     -h, --help        Show this help

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
CLEAN=0
FLASH=0
MONITOR=0
SERIAL_PORT=""
CONFIG_FILE="./config.cvs"
IDF_PATH="${HOME}/esp/esp-idf"
PROJECT_DIR="$(pwd)"

# Function to print colored messages
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show help
show_help() {
    cat << EOF
ESP-Miner Build Script

Usage: ./build.sh [options]

Options:
    -c, --clean         Full clean before build
    -f, --flash         Flash after successful build
    -m, --monitor       Open serial monitor after flash
    -p, --port PORT     Serial port (default: auto-detect)
    --config FILE       Config file path (default: ./config.cvs)
    -h, --help          Show this help

Examples:
    ./build.sh                      # Just build
    ./build.sh -c -f -m             # Clean, build, flash, and monitor
    ./build.sh -f -p /dev/cu.usbmodem141201  # Build and flash to specific port
    ./build.sh --clean --flash --config my-config.cvs  # Custom config

EOF
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN=1
            shift
            ;;
        -f|--flash)
            FLASH=1
            shift
            ;;
        -m|--monitor)
            MONITOR=1
            shift
            ;;
        -p|--port)
            SERIAL_PORT="$2"
            shift 2
            ;;
        --config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            ;;
    esac
done

# Header
echo "=========================================="
echo "  ESP-Miner Build Script"
echo "=========================================="
echo ""

# Check if ESP-IDF exists
if [ ! -d "$IDF_PATH" ]; then
    print_error "ESP-IDF not found at $IDF_PATH"
    print_info "Please install ESP-IDF v5.3 first"
    exit 1
fi

# Check if we're in the ESP-Miner directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "main" ]; then
    print_error "Not in ESP-Miner project directory"
    print_info "Please run this script from the ESP-Miner root directory"
    exit 1
fi

# Source ESP-IDF environment
print_info "Setting up ESP-IDF environment..."
cd "$IDF_PATH"
. ./export.sh
cd "$PROJECT_DIR"

# Verify IDF version
IDF_VERSION=$(idf.py --version 2>&1 | grep -o "v5\.[0-9]*" | head -1)
if [[ ! "$IDF_VERSION" =~ v5\.3 ]]; then
    print_warning "Expected ESP-IDF v5.3, found $IDF_VERSION"
    print_info "Continuing anyway..."
fi

print_success "ESP-IDF environment ready"

# Clean if requested
if [ $CLEAN -eq 1 ]; then
    print_info "Performing full clean..."
    rm -rf build managed_components dependencies.lock
    idf.py fullclean
    print_success "Clean complete"
fi

# Build firmware
print_info "Building firmware..."
print_info "This may take several minutes on first build..."

if idf.py build; then
    print_success "Build completed successfully"
else
    print_error "Build failed"
    exit 1
fi

# Create merged binary
print_info "Creating merged binary..."
if idf.py merge-bin; then
    print_success "Merged binary created: build/esp-miner-merged.bin"
    
    # Show binary size
    BINARY_SIZE=$(ls -lh build/esp-miner-merged.bin | awk '{print $5}')
    print_info "Binary size: $BINARY_SIZE"
else
    print_error "Failed to create merged binary"
    exit 1
fi

# Flash if requested
if [ $FLASH -eq 1 ]; then
    # Auto-detect serial port if not specified
    if [ -z "$SERIAL_PORT" ]; then
        print_info "Auto-detecting serial port..."
        
        # Look for common ESP32-S3 USB-JTAG ports
        for port in /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART /dev/cu.usbserial*; do
            if [ -e "$port" ]; then
                SERIAL_PORT="$port"
                print_info "Found serial port: $SERIAL_PORT"
                break
            fi
        done
        
        if [ -z "$SERIAL_PORT" ]; then
            print_error "Could not auto-detect serial port"
            print_info "Available ports:"
            ls -1 /dev/cu.* 2>/dev/null || echo "  None found"
            print_info "Please specify port with -p option"
            exit 1
        fi
    fi
    
    # Check if config file exists
    if [ ! -f "$CONFIG_FILE" ]; then
        print_warning "Config file not found: $CONFIG_FILE"
        print_info "Flashing without config (you'll need to configure via web UI)"
        CONFIG_PARAM=""
    else
        print_info "Using config file: $CONFIG_FILE"
        CONFIG_PARAM="--config $CONFIG_FILE"
    fi
    
    # Check if bitaxetool is installed
    if ! command -v bitaxetool &> /dev/null; then
        print_warning "bitaxetool not found, installing..."
        pip3 install bitaxetool
    fi
    
    # Flash the firmware
    print_info "Flashing to $SERIAL_PORT..."
    print_info "This will take about 60-90 seconds..."
    
    if bitaxetool --port "$SERIAL_PORT" \
                  $CONFIG_PARAM \
                  --firmware ./build/esp-miner-merged.bin; then
        print_success "Flash complete!"
        
        # Wait for device to reboot
        print_info "Waiting for device to reboot..."
        sleep 3
    else
        print_error "Flash failed"
        print_info "Try:"
        print_info "  1. Different USB cable"
        print_info "  2. Different USB port"
        print_info "  3. Power cycle the device"
        exit 1
    fi
fi

# Monitor if requested
if [ $MONITOR -eq 1 ]; then
    if [ -z "$SERIAL_PORT" ]; then
        print_error "No serial port specified for monitoring"
        print_info "Use -p option to specify port"
        exit 1
    fi
    
    print_info "Opening serial monitor on $SERIAL_PORT"
    print_info "Press Ctrl+] to exit"
    echo ""
    
    idf.py -p "$SERIAL_PORT" monitor
fi

# Summary
echo ""
echo "=========================================="
print_success "Build process complete!"
echo "=========================================="
echo ""

if [ $FLASH -eq 1 ]; then
    print_info "Next steps:"
    echo "  1. Wait for device to connect to WiFi"
    echo "  2. Check serial output for IP address"
    echo "  3. Access web UI at http://[IP_ADDRESS]"
    echo ""
    
    if [ $MONITOR -eq 0 ]; then
        print_info "To monitor serial output, run:"
        echo "  idf.py -p $SERIAL_PORT monitor"
        echo ""
    fi
else
    print_info "To flash the firmware, run:"
    echo "  ./build.sh --flash"
    echo "  or"
    echo "  bitaxetool --port /dev/cu.usbmodemXXXXXX --config config.cvs --firmware build/esp-miner-merged.bin"
    echo ""
fi
