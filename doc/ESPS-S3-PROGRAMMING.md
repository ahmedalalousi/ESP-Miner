# ESP32-S3 Programming Guide

Comprehensive guide to understanding and programming the ESP32-S3 microcontroller.

## Table of Contents
1. [ESP32-S3 Architecture](#esp32-s3-architecture)
2. [Programming Fundamentals](#programming-fundamentals)
3. [Development Workflow](#development-workflow)
4. [Memory Architecture](#memory-architecture)
5. [Boot Process](#boot-process)
6. [Flashing Process](#flashing-process)

---

## ESP32-S3 Architecture

### Core Specifications

**Your Hardware: ESP32-S3-WROOM-1-N16R8**
- **CPU**: Dual-core Xtensa LX7 @ 240MHz
- **Flash**: 16MB (N16)
- **PSRAM**: 8MB Octal PSRAM (R8)
- **RAM**: 512KB SRAM
- **WiFi**: 802.11 b/g/n (2.4GHz)
- **Bluetooth**: BLE 5.0
- **USB**: Native USB-OTG with USB-JTAG for programming

### Block Diagram

```
┌─────────────────────────────────────────────────┐
│          ESP32-S3-WROOM-1-N16R8                 │
├─────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐            │
│  │ Xtensa LX7   │  │ Xtensa LX7   │            │
│  │ Core 0       │  │ Core 1       │            │
│  │ (Protocol)   │  │ (Application)│            │
│  └──────┬───────┘  └──────┬───────┘            │
│         └──────────────────┘                    │
│                │                                 │
│  ┌─────────────┴────────────────┐               │
│  │      512KB SRAM              │               │
│  │  (Internal RAM - Fast)       │               │
│  └─────────────┬────────────────┘               │
│                │                                 │
│  ┌─────────────┴────────────────┐               │
│  │      8MB PSRAM (External)    │               │
│  │  (Slower, for large buffers) │               │
│  └─────────────┬────────────────┘               │
│                │                                 │
│  ┌─────────────┴────────────────┐               │
│  │   16MB Flash (External SPI)  │               │
│  │  - Bootloader                │               │
│  │  - Partition Table           │               │
│  │  - Application Code          │               │
│  │  - NVS (Config Storage)      │               │
│  │  - Web UI Assets             │               │
│  └──────────────────────────────┘               │
│                                                  │
│  ┌──────────────────────────────┐               │
│  │  Peripherals                 │               │
│  │  - GPIO (45 pins)            │               │
│  │  - I2C x2                    │               │
│  │  - SPI x4                    │               │
│  │  - UART x3                   │               │
│  │  - ADC, PWM, etc.            │               │
│  └──────────────────────────────┘               │
│                                                  │
│  ┌──────────────────────────────┐               │
│  │  Connectivity                │               │
│  │  - WiFi Radio                │               │
│  │  - Bluetooth LE 5.0          │               │
│  │  - USB-OTG + USB-JTAG        │               │
│  └──────────────────────────────┘               │
└──────────────────────────────────────────────────┘
```

---

## Programming Fundamentals

### Programming Languages

**ESP32-S3 supports multiple programming methods**:

1. **C/C++ with ESP-IDF** (What ESP-Miner uses)
   - Professional development
   - Full hardware control
   - Best performance
   - Steeper learning curve

2. **Arduino Framework**
   - Easier for beginners
   - Limited functionality
   - Good for simple projects

3. **MicroPython**
   - Python scripting
   - Interpreted (slower)
   - Rapid prototyping

4. **Rust**
   - Memory safety
   - Growing ecosystem
   - Modern approach

**ESP-Miner uses ESP-IDF (C/C++)** for maximum performance and hardware control.

### ESP-IDF Framework Architecture

```
Your Application Code
        ↓
ESP-IDF Components (WiFi, HTTP, etc.)
        ↓
FreeRTOS (Real-Time Operating System)
        ↓
Hardware Abstraction Layer (HAL)
        ↓
ESP32-S3 Hardware
```

### Key Concepts

#### 1. **FreeRTOS Tasks**

ESP-IDF uses FreeRTOS for multitasking:

```c
// Create a task
void my_task_function(void *pvParameters) {
    while(1) {
        // Do work
        printf("Task running\n");
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second
    }
}

// Start the task
xTaskCreate(
    my_task_function,   // Task function
    "MyTask",           // Task name
    4096,               // Stack size (bytes)
    NULL,               // Parameters
    5,                  // Priority
    NULL                // Task handle
);
```

**ESP-Miner has multiple tasks**:
- `stratum_task` - Communicates with mining pool
- `asic_task` - Controls mining ASIC
- `power_management_task` - Manages voltage/frequency
- `statistics_task` - Collects and reports statistics

#### 2. **Event-Driven Programming**

ESP-IDF uses events for communication:

```c
// Define event
ESP_EVENT_DEFINE_BASE(MY_EVENTS);

// Post event
esp_event_post(MY_EVENTS, EVENT_ID, &data, sizeof(data), portMAX_DELAY);

// Handle event
static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data) {
    if (event_id == EVENT_ID) {
        // Handle the event
    }
}

// Register handler
esp_event_handler_register(MY_EVENTS, EVENT_ID, event_handler, NULL);
```

#### 3. **Component-Based Architecture**

ESP-IDF projects are organised into components:

```
ESP-Miner/
├── main/                    # Main application
├── components/              # Custom components
│   ├── asic/               # ASIC control
│   ├── stratum/            # Mining protocol
│   ├── power/              # Power management
│   └── thermal/            # Temperature control
└── managed_components/      # External dependencies
```

Each component has:
- `CMakeLists.txt` - Build configuration
- `include/` - Public headers
- `*.c/*.cpp` - Implementation

---

## Development Workflow

### 1. Project Structure

```
ESP-Miner/
├── CMakeLists.txt           # Top-level build config
├── sdkconfig                # Project configuration
├── main/
│   ├── CMakeLists.txt       # Main component build
│   ├── main.c               # Application entry point
│   └── Kconfig.projbuild    # Configuration menu
├── components/
│   └── my_component/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── my_component.h
│       └── my_component.c
└── build/                   # Build output (generated)
```

### 2. Application Entry Point

**`main/main.c`** is where your programme starts:

```c
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "Application starting");
    
    // Initialise peripherals
    gpio_init();
    i2c_init();
    wifi_init();
    
    // Create tasks
    xTaskCreate(my_task, "MyTask", 4096, NULL, 5, NULL);
    
    // Main task ends, FreeRTOS scheduler continues
}
```

### 3. Configuration System

**menuconfig** creates `sdkconfig`:

```bash
idf.py menuconfig
```

Access config values in code:

```c
#include "sdkconfig.h"

#ifdef CONFIG_MY_FEATURE_ENABLED
    enable_feature();
#endif

int buffer_size = CONFIG_MY_BUFFER_SIZE;
```

### 4. Build Process

```bash
# Configure
idf.py menuconfig

# Build
idf.py build

# Result: build/esp-miner.bin
```

**What happens during build**:
1. CMake generates build system
2. Components are compiled
3. Libraries are linked
4. Partition table is generated
5. Bootloader is built
6. Final binary is created

---

## Memory Architecture

### Memory Types

**1. Internal SRAM (512KB) - Fastest**
- Used for: Active variables, stack, heap
- Speed: ~160 MHz access
- Location: `0x3FC88000` - `0x3FD00000`

**2. External PSRAM (8MB) - Slower**
- Used for: Large buffers, web UI assets
- Speed: ~80 MHz access
- Must be explicitly enabled

**3. Flash (16MB) - Slowest for execution**
- Used for: Code, constants, assets
- Execution: Code is cached in SRAM
- Direct access: Read-only data

### Memory Allocation

```c
// Allocate in SRAM (default)
void *ptr1 = malloc(1024);

// Allocate in PSRAM (if available)
void *ptr2 = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);

// Allocate DMA-capable memory (required for some peripherals)
void *dma_buf = heap_caps_malloc(1024, MALLOC_CAP_DMA);

// Free memory
free(ptr1);
free(ptr2);
free(dma_buf);
```

### Memory Debugging

```c
// Check free memory
size_t free_sram = esp_get_free_heap_size();
size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

ESP_LOGI(TAG, "Free SRAM: %u bytes", free_sram);
ESP_LOGI(TAG, "Free PSRAM: %u bytes", free_psram);
```

---

## Boot Process

### Boot Sequence

```
1. ROM Bootloader (in chip ROM)
   ↓
   - Initialises basic hardware
   - Reads flash at 0x1000 (bootloader location)
   ↓
2. Second-Stage Bootloader (in flash)
   ↓
   - Initialises flash
   - Reads partition table (0x8000)
   - Validates application
   - Initialises PSRAM
   ↓
3. Application (your code)
   ↓
   - app_main() is called
   - FreeRTOS scheduler starts
```

### Partition Table

Located at `0x8000` in flash:

```
# Name,      Type, SubType,  Offset,   Size
nvs,         data, nvs,      0x9000,   0x6000
phy_init,    data, phy,      0xf000,   0x1000
factory,     app,  factory,  0x10000,  0x400000
www,         data, spiffs,   0x410000, 0x300000
ota_0,       app,  ota_0,    0x710000, 0x400000
ota_1,       app,  ota_1,    0xb10000, 0x400000
otadata,     data, ota,      0xf10000, 0x2000
coredump,    data, coredump, 0xf12000, 0x10000
```

**Partition purposes**:
- `nvs`: Configuration storage (WiFi, settings)
- `factory`: Main application
- `www`: Web UI files (SPIFFS filesystem)
- `ota_0/ota_1`: Over-the-air update slots
- `otadata`: Tracks which OTA is active

---

## Flashing Process

### How Flashing Works

**1. Enter Bootloader Mode**
- ESP32-S3 has USB-JTAG built-in
- ROM bootloader is always in chip
- Automatically enters download mode on USB connection

**2. Erase Flash**
```
esptool.py erase_region 0x0 0x1000000
```

**3. Write Bootloader**
```
esptool.py write_flash 0x0 bootloader.bin
```

**4. Write Partition Table**
```
esptool.py write_flash 0x8000 partition-table.bin
```

**5. Write Application**
```
esptool.py write_flash 0x10000 app.bin
```

**6. Write Additional Partitions**
```
esptool.py write_flash 0x410000 www.bin
```

### Merged Binary

ESP-Miner uses a **merged binary** that combines everything:

```
esp-miner-merged.bin @ 0x0
├── Bootloader @ 0x0
├── Partition Table @ 0x8000
├── NVS Data @ 0x9000 (from config.cvs)
├── Application @ 0x10000
└── Web UI @ 0x410000
```

Flash in one command:
```bash
esptool.py write_flash 0x0 esp-miner-merged.bin
```

---

## Programming Examples

### Example 1: Simple GPIO Control

```c
#include "driver/gpio.h"

#define LED_PIN GPIO_NUM_2

void app_main(void) {
    // Configure GPIO as output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Blink LED
    while(1) {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Example 2: I2C Communication

```c
#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO    48
#define I2C_MASTER_SDA_IO    47
#define I2C_MASTER_FREQ_HZ   100000

void i2c_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

// Read from I2C device
uint8_t i2c_read_byte(uint8_t device_addr, uint8_t reg_addr) {
    uint8_t data;
    i2c_master_write_read_device(I2C_NUM_0, device_addr,
                                 &reg_addr, 1, &data, 1,
                                 pdMS_TO_TICKS(1000));
    return data;
}
```

### Example 3: WiFi Connection

```c
#include "esp_wifi.h"
#include "esp_event.h"

void wifi_init(void) {
    // Initialise NVS (required for WiFi)
    nvs_flash_init();
    
    // Initialise network interface
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    
    // WiFi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "YourSSID",
            .password = "YourPassword",
        },
    };
    
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}
```

---

## Debugging

### Serial Output

```c
#include "esp_log.h"

static const char *TAG = "myapp";

ESP_LOGI(TAG, "Info message: %d", value);
ESP_LOGW(TAG, "Warning message");
ESP_LOGE(TAG, "Error message");
ESP_LOGD(TAG, "Debug message");  // Only in debug builds
```

View output:
```bash
idf.py monitor
```

### GDB Debugging

```bash
# Start OpenOCD (in one terminal)
openocd -f board/esp32s3-builtin.cfg

# Start GDB (in another terminal)
xtensa-esp32s3-elf-gdb build/esp-miner.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) break app_main
(gdb) continue
```

---

## Resources

- **Official ESP-IDF Docs**: https://docs.espressif.com/projects/esp-idf/
- **ESP32-S3 Datasheet**: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- **ESP32-S3 Technical Reference**: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
- **FreeRTOS Documentation**: https://www.freertos.org/Documentation/RTOS_book.html

---

**Next**: Understanding webapp interaction with ESP32-S3
