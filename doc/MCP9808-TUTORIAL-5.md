# MCP9808 Temperature Sensor Tutorial - Part 5: Advanced Usage

**Learning Goals**: Device enumeration and management, multi-device support across I2C/SPI/UART interfaces, and implementing robust serial communication protocols.

---

## Table of Contents

**Section A**: Multiple I2C Devices - Theory and Practice
**Section B**: Device Enumeration Patterns
**Section C**: SPI Device Management
**Section D**: UART Communication Protocols
**Section E**: Protocol Design and Implementation

---

# Section A: Multiple I2C Devices

## The Problem: One Driver, Multiple Sensors

Current limitation in our driver:

```c
// ❌ PROBLEM: Hardcoded single device
#define MCP9808_I2C_ADDR_DEFAULT 0x18

static esp_err_t mcp9808_read_register(uint8_t reg_addr, uint16_t *data) {
    // Always talks to 0x18!
    esp_err_t ret = i2c_master_write_read_device(
        I2C_NUM_0,
        MCP9808_I2C_ADDR_DEFAULT,  // ← Hardcoded address
        &reg_addr,
        1,
        read_buf,
        2,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
}
```

**What we need**:
- Support multiple sensors at different addresses
- Each sensor has independent state
- Clean API for managing multiple devices

---

## Understanding I2C Addressing

### Physical Address Configuration

MCP9808 has address pins A0, A1, A2:

```
Base address: 0011000 (0x18)

A2 A1 A0  | Address | Binary
----------|---------|----------
0  0  0   | 0x18    | 0011000
0  0  1   | 0x19    | 0011001
0  1  0   | 0x1A    | 0011010
0  1  1   | 0x1B    | 0011011
1  0  0   | 0x1C    | 0011100
1  0  1   | 0x1D    | 0011101
1  1  0   | 0x1E    | 0011110
1  1  1   | 0x1F    | 0011111
```

**Hardware setup for 3 sensors**:
```
Sensor 1: A2=0, A1=0, A0=0  → Address 0x18
Sensor 2: A2=0, A1=0, A0=1  → Address 0x19  (A0 → VDD)
Sensor 3: A2=0, A1=1, A0=0  → Address 0x1A  (A1 → VDD)
```

**Wiring**:
```
         ESP32
           │
    ┌──────┼──────┐
    │      │      │
  ┌─┴─┐  ┌─┴─┐  ┌─┴─┐
  │0x18│ │0x19│ │0x1A│
  └───┘  └───┘  └───┘
   All share same SDA/SCL bus
```

---

## Design Pattern 1: Device Handle

### Concept: Instance-Based API

Instead of global state, create instances:

```c
// Old (single device):
mcp9808_init();
mcp9808_read_temperature(&temp);

// New (multiple devices):
mcp9808_handle_t sensor1 = mcp9808_create(0x18);
mcp9808_handle_t sensor2 = mcp9808_create(0x19);
mcp9808_read_temperature(sensor1, &temp1);
mcp9808_read_temperature(sensor2, &temp2);
```

**Benefits**:
- Each sensor independent
- Clear which sensor you're talking to
- Can have different configurations
- Easy to add/remove sensors

### Implementation: Device Handle Structure

**`components/mcp9808/include/mcp9808.h`** - Add:

```c
// Opaque handle - users don't see internals
typedef struct mcp9808_dev_t* mcp9808_handle_t;

// Configuration for each device
typedef struct {
    uint8_t i2c_addr;           // Device I2C address (0x18-0x1F)
    i2c_port_t i2c_port;        // Which I2C bus (I2C_NUM_0 or I2C_NUM_1)
    uint16_t config_reg;        // Configuration register value
} mcp9808_config_t;

// Default configuration
#define MCP9808_CONFIG_DEFAULT() { \
    .i2c_addr = 0x18, \
    .i2c_port = I2C_NUM_0, \
    .config_reg = 0x0000 \
}

/**
 * @brief Create and initialize MCP9808 device instance
 * 
 * @param config Device configuration
 * @return Device handle, or NULL on failure
 * 
 * Example:
 *   mcp9808_config_t cfg = MCP9808_CONFIG_DEFAULT();
 *   cfg.i2c_addr = 0x19;
 *   mcp9808_handle_t sensor = mcp9808_create(&cfg);
 */
mcp9808_handle_t mcp9808_create(const mcp9808_config_t *config);

/**
 * @brief Delete device instance and free resources
 * 
 * @param handle Device handle
 * @return ESP_OK on success
 */
esp_err_t mcp9808_delete(mcp9808_handle_t handle);

/**
 * @brief Read temperature from specific device
 * 
 * @param handle Device handle
 * @param temperature Pointer to store temperature
 * @return ESP_OK on success
 */
esp_err_t mcp9808_read_temperature(mcp9808_handle_t handle, float *temperature);

/**
 * @brief Check if specific device is present
 * 
 * @param handle Device handle
 * @return true if device responds
 */
bool mcp9808_is_present(mcp9808_handle_t handle);
```

**Why opaque handle?**

```c
// In header: Users only see pointer
typedef struct mcp9808_dev_t* mcp9808_handle_t;

// In .c file: We define the actual structure
struct mcp9808_dev_t {
    uint8_t i2c_addr;
    i2c_port_t i2c_port;
    uint16_t config_reg;
    // Internal state
    float last_temperature;
    uint64_t last_read_time;
    uint32_t read_count;
    uint32_t error_count;
};
```

**Benefits**:
- Users can't access internals directly
- Can change structure without breaking API
- Forced to use provided functions
- Better encapsulation

### Implementation: Device Management

**`components/mcp9808/mcp9808.c`**:

```c
#include "mcp9808.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "mcp9808";

// Internal device structure
struct mcp9808_dev_t {
    uint8_t i2c_addr;
    i2c_port_t i2c_port;
    uint16_t config_reg;
    
    // Statistics
    float last_temperature;
    uint64_t last_read_time;
    uint32_t read_count;
    uint32_t error_count;
};

// Internal helper - write register to specific device
static esp_err_t mcp9808_write_reg(mcp9808_handle_t handle, 
                                    uint8_t reg_addr, 
                                    uint16_t data) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t write_buf[3];
    write_buf[0] = reg_addr;
    write_buf[1] = (data >> 8) & 0xFF;  // MSB
    write_buf[2] = data & 0xFF;          // LSB
    
    esp_err_t ret = i2c_master_write_to_device(
        handle->i2c_port,      // ← Use device's I2C port
        handle->i2c_addr,      // ← Use device's address
        write_buf,
        3,
        pdMS_TO_TICKS(1000)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device 0x%02X: Write reg 0x%02X failed: %s", 
                 handle->i2c_addr, reg_addr, esp_err_to_name(ret));
    }
    
    return ret;
}

// Internal helper - read register from specific device
static esp_err_t mcp9808_read_reg(mcp9808_handle_t handle,
                                   uint8_t reg_addr,
                                   uint16_t *data) {
    if (handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t read_buf[2];
    
    esp_err_t ret = i2c_master_write_read_device(
        handle->i2c_port,      // ← Use device's I2C port
        handle->i2c_addr,      // ← Use device's address
        &reg_addr,
        1,
        read_buf,
        2,
        pdMS_TO_TICKS(1000)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device 0x%02X: Read reg 0x%02X failed: %s",
                 handle->i2c_addr, reg_addr, esp_err_to_name(ret));
        return ret;
    }
    
    *data = (read_buf[0] << 8) | read_buf[1];
    return ESP_OK;
}

// Create device instance
mcp9808_handle_t mcp9808_create(const mcp9808_config_t *config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }
    
    // Allocate device structure
    mcp9808_handle_t handle = malloc(sizeof(struct mcp9808_dev_t));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate device handle");
        return NULL;
    }
    
    // Initialize structure
    memset(handle, 0, sizeof(struct mcp9808_dev_t));
    handle->i2c_addr = config->i2c_addr;
    handle->i2c_port = config->i2c_port;
    handle->config_reg = config->config_reg;
    
    ESP_LOGI(TAG, "Checking device at address 0x%02X on I2C port %d",
             handle->i2c_addr, handle->i2c_port);
    
    // Verify device is present
    uint8_t dummy;
    esp_err_t ret = i2c_master_read_from_device(
        handle->i2c_port,
        handle->i2c_addr,
        &dummy,
        1,
        pdMS_TO_TICKS(100)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device not found at 0x%02X", handle->i2c_addr);
        free(handle);
        return NULL;
    }
    
    // Verify manufacturer ID
    uint16_t mfg_id;
    ret = mcp9808_read_reg(handle, MCP9808_REG_MANUFACTURER_ID, &mfg_id);
    if (ret != ESP_OK || mfg_id != MCP9808_MANUFACTURER_ID) {
        ESP_LOGE(TAG, "Invalid manufacturer ID: 0x%04X (expected 0x%04X)",
                 mfg_id, MCP9808_MANUFACTURER_ID);
        free(handle);
        return NULL;
    }
    
    // Verify device ID
    uint16_t dev_id;
    ret = mcp9808_read_reg(handle, MCP9808_REG_DEVICE_ID, &dev_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID");
        free(handle);
        return NULL;
    }
    
    uint8_t device_id_upper = (dev_id >> 8) & 0xFF;
    if (device_id_upper != MCP9808_DEVICE_ID) {
        ESP_LOGE(TAG, "Invalid device ID: 0x%02X (expected 0x%02X)",
                 device_id_upper, MCP9808_DEVICE_ID);
        free(handle);
        return NULL;
    }
    
    // Configure device
    ret = mcp9808_write_reg(handle, MCP9808_REG_CONFIG, handle->config_reg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure device");
        free(handle);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Device 0x%02X initialized successfully", handle->i2c_addr);
    return handle;
}

// Delete device instance
esp_err_t mcp9808_delete(mcp9808_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Deleting device 0x%02X", handle->i2c_addr);
    free(handle);
    return ESP_OK;
}

// Read temperature from specific device
esp_err_t mcp9808_read_temperature(mcp9808_handle_t handle, float *temperature) {
    if (handle == NULL || temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint16_t raw_temp;
    esp_err_t ret = mcp9808_read_reg(handle, MCP9808_REG_TEMP_AMBIENT, &raw_temp);
    
    if (ret != ESP_OK) {
        handle->error_count++;
        return ret;
    }
    
    // Clear flag bits
    raw_temp = raw_temp & 0x1FFF;
    
    // Convert to temperature
    if (raw_temp & 0x1000) {
        // Negative temperature
        raw_temp = raw_temp & 0x0FFF;
        *temperature = -(float)raw_temp * 0.0625f;
    } else {
        // Positive temperature
        *temperature = (float)raw_temp * 0.0625f;
    }
    
    // Update statistics
    handle->last_temperature = *temperature;
    handle->last_read_time = esp_timer_get_time() / 1000;
    handle->read_count++;
    
    ESP_LOGD(TAG, "Device 0x%02X: Temperature = %.2f°C",
             handle->i2c_addr, *temperature);
    
    return ESP_OK;
}

// Check if device is present
bool mcp9808_is_present(mcp9808_handle_t handle) {
    if (handle == NULL) {
        return false;
    }
    
    uint8_t dummy;
    esp_err_t ret = i2c_master_read_from_device(
        handle->i2c_port,
        handle->i2c_addr,
        &dummy,
        1,
        pdMS_TO_TICKS(100)
    );
    
    return (ret == ESP_OK);
}
```

**Key concepts explained**:

1. **malloc() and free()**:
   - `malloc()`: Allocate memory from heap
   - `free()`: Return memory to heap
   - Why: Number of devices unknown at compile time

2. **Handle pattern**:
   ```c
   // User sees:
   mcp9808_handle_t sensor;  // Just a pointer
   
   // We manage:
   struct mcp9808_dev_t {    // Actual data
       // ... fields ...
   };
   ```

3. **Per-device state**:
   - Each handle has its own statistics
   - Independent error counting
   - Own configuration

### Usage Example

**`main/main.c`**:

```c
#include "mcp9808.h"

void app_main(void) {
    // Initialize I2C (same as before)
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 47,
        .scl_io_num = 48,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    
    // Create multiple sensors
    mcp9808_config_t cfg1 = MCP9808_CONFIG_DEFAULT();
    cfg1.i2c_addr = 0x18;
    mcp9808_handle_t sensor1 = mcp9808_create(&cfg1);
    
    mcp9808_config_t cfg2 = MCP9808_CONFIG_DEFAULT();
    cfg2.i2c_addr = 0x19;
    mcp9808_handle_t sensor2 = mcp9808_create(&cfg2);
    
    mcp9808_config_t cfg3 = MCP9808_CONFIG_DEFAULT();
    cfg3.i2c_addr = 0x1A;
    mcp9808_handle_t sensor3 = mcp9808_create(&cfg3);
    
    // Check which sensors initialized successfully
    if (sensor1 == NULL) ESP_LOGW("main", "Sensor 0x18 not found");
    if (sensor2 == NULL) ESP_LOGW("main", "Sensor 0x19 not found");
    if (sensor3 == NULL) ESP_LOGW("main", "Sensor 0x1A not found");
    
    // Read temperatures
    while (1) {
        float temp1, temp2, temp3;
        
        if (sensor1 && mcp9808_read_temperature(sensor1, &temp1) == ESP_OK) {
            ESP_LOGI("main", "Sensor 1: %.2f°C", temp1);
        }
        
        if (sensor2 && mcp9808_read_temperature(sensor2, &temp2) == ESP_OK) {
            ESP_LOGI("main", "Sensor 2: %.2f°C", temp2);
        }
        
        if (sensor3 && mcp9808_read_temperature(sensor3, &temp3) == ESP_OK) {
            ESP_LOGI("main", "Sensor 3: %.2f°C", temp3);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    // Clean up (if ever exiting)
    mcp9808_delete(sensor1);
    mcp9808_delete(sensor2);
    mcp9808_delete(sensor3);
}
```

---

# Section B: Device Enumeration Patterns

## Problem: Discovering Devices at Runtime

**Scenario**: You don't know which addresses have sensors connected.

**Solution**: Scan the bus!

### I2C Bus Scanner

```c
/**
 * @brief Scan I2C bus for MCP9808 devices
 * 
 * @param i2c_port Which I2C port to scan
 * @param found_addrs Array to store found addresses
 * @param max_devices Maximum devices to find
 * @return Number of devices found
 * 
 * Example:
 *   uint8_t addrs[8];
 *   int count = mcp9808_scan_bus(I2C_NUM_0, addrs, 8);
 *   ESP_LOGI(TAG, "Found %d sensors", count);
 */
int mcp9808_scan_bus(i2c_port_t i2c_port, uint8_t *found_addrs, int max_devices);
```

**Implementation**:

```c
int mcp9808_scan_bus(i2c_port_t i2c_port, uint8_t *found_addrs, int max_devices) {
    if (found_addrs == NULL || max_devices <= 0) {
        return 0;
    }
    
    int found_count = 0;
    
    ESP_LOGI(TAG, "Scanning I2C port %d for MCP9808 devices...", i2c_port);
    
    // Scan valid MCP9808 address range (0x18 - 0x1F)
    for (uint8_t addr = 0x18; addr <= 0x1F && found_count < max_devices; addr++) {
        uint8_t dummy;
        
        // Try to read from device
        esp_err_t ret = i2c_master_read_from_device(
            i2c_port,
            addr,
            &dummy,
            1,
            pdMS_TO_TICKS(50)  // Short timeout
        );
        
        if (ret == ESP_OK) {
            // Device responded, verify it's actually MCP9808
            uint16_t mfg_id;
            uint8_t reg_addr = MCP9808_REG_MANUFACTURER_ID;
            uint8_t read_buf[2];
            
            ret = i2c_master_write_read_device(
                i2c_port,
                addr,
                &reg_addr,
                1,
                read_buf,
                2,
                pdMS_TO_TICKS(100)
            );
            
            if (ret == ESP_OK) {
                mfg_id = (read_buf[0] << 8) | read_buf[1];
                
                if (mfg_id == MCP9808_MANUFACTURER_ID) {
                    found_addrs[found_count++] = addr;
                    ESP_LOGI(TAG, "  Found MCP9808 at 0x%02X", addr);
                } else {
                    ESP_LOGD(TAG, "  Device at 0x%02X is not MCP9808 (mfg=0x%04X)", 
                             addr, mfg_id);
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "Scan complete: %d MCP9808 device(s) found", found_count);
    return found_count;
}
```

**Why verify manufacturer ID?**
- Many I2C devices might respond
- Could be different sensor at that address
- Prevents false positives

### Device Registry Pattern

For managing discovered devices:

```c
// Device registry
typedef struct {
    mcp9808_handle_t handles[8];  // Up to 8 devices
    int count;
} mcp9808_registry_t;

/**
 * @brief Auto-discover and initialize all MCP9808 devices
 * 
 * @param registry Registry to populate
 * @param i2c_port I2C port to scan
 * @return Number of devices initialized
 */
int mcp9808_auto_init(mcp9808_registry_t *registry, i2c_port_t i2c_port) {
    if (registry == NULL) {
        return 0;
    }
    
    // Clear registry
    memset(registry, 0, sizeof(mcp9808_registry_t));
    
    // Scan for devices
    uint8_t found_addrs[8];
    int found_count = mcp9808_scan_bus(i2c_port, found_addrs, 8);
    
    // Initialize each found device
    for (int i = 0; i < found_count; i++) {
        mcp9808_config_t cfg = MCP9808_CONFIG_DEFAULT();
        cfg.i2c_addr = found_addrs[i];
        cfg.i2c_port = i2c_port;
        
        mcp9808_handle_t handle = mcp9808_create(&cfg);
        if (handle != NULL) {
            registry->handles[registry->count++] = handle;
            ESP_LOGI(TAG, "Initialized sensor %d at 0x%02X", 
                     registry->count, found_addrs[i]);
        }
    }
    
    return registry->count;
}

/**
 * @brief Read all sensors in registry
 * 
 * @param registry Device registry
 * @param temps Array to store temperatures
 * @param max_temps Size of temps array
 * @return Number of successful reads
 */
int mcp9808_read_all(mcp9808_registry_t *registry, float *temps, int max_temps) {
    if (registry == NULL || temps == NULL) {
        return 0;
    }
    
    int read_count = 0;
    int count = (registry->count < max_temps) ? registry->count : max_temps;
    
    for (int i = 0; i < count; i++) {
        if (mcp9808_read_temperature(registry->handles[i], &temps[i]) == ESP_OK) {
            read_count++;
        }
    }
    
    return read_count;
}
```

**Usage**:

```c
void app_main(void) {
    // Initialize I2C
    // ... (same as before) ...
    
    // Auto-discover sensors
    mcp9808_registry_t registry;
    int count = mcp9808_auto_init(&registry, I2C_NUM_0);
    
    ESP_LOGI("main", "Found and initialized %d sensors", count);
    
    // Read all sensors
    while (1) {
        float temps[8];
        int read_count = mcp9808_read_all(&registry, temps, 8);
        
        for (int i = 0; i < read_count; i++) {
            ESP_LOGI("main", "Sensor %d: %.2f°C", i, temps[i]);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

---

# Section C: SPI Device Management

## SPI vs I2C: Key Differences

| Feature | I2C | SPI |
|---------|-----|-----|
| **Addressing** | 7-bit address in protocol | Chip Select (CS) pin per device |
| **Speed** | Up to 3.4 Mbps | Up to 80 MHz on ESP32 |
| **Wires** | 2 (SDA, SCL) | 4+ (MISO, MOSI, CLK, CS) |
| **Multi-device** | Share same bus | Share data lines, separate CS |

### SPI Bus Topology

```
         ESP32
           │
    ┌──────┼──────┬───────┐
    │      │      │       │
  MOSI   MISO   CLK      │
    │      │      │       │
    └──────┴──────┴───────┤
                          │
            ┌─────────────┼─────────────┐
            │             │             │
         CS_0          CS_1          CS_2
            │             │             │
        ┌───▼───┐     ┌───▼───┐     ┌───▼───┐
        │Device1│     │Device2│     │Device3│
        └───────┘     └───────┘     └───────┘
```

**Key point**: CS (Chip Select) determines which device is active.

### SPI Device Handle Pattern

Similar to I2C, but with CS pin management:

```c
// SPI device configuration
typedef struct {
    spi_host_device_t host;     // SPI2_HOST or SPI3_HOST
    int cs_pin;                 // GPIO for chip select
    int clock_speed_hz;         // SPI clock frequency
    uint8_t mode;               // SPI mode (0-3)
} spi_device_config_t;

// Device handle (includes SPI handle)
typedef struct {
    spi_device_handle_t spi_handle;  // ESP-IDF SPI handle
    int cs_pin;
    // Device-specific state
} my_spi_device_t;
```

### Example: BME280 Sensor on SPI

**Header** (`bme280_spi.h`):

```c
typedef struct bme280_dev_t* bme280_handle_t;

typedef struct {
    spi_host_device_t host;
    int cs_pin;
    int clock_speed_hz;
} bme280_config_t;

#define BME280_CONFIG_DEFAULT() { \
    .host = SPI2_HOST, \
    .cs_pin = 10, \
    .clock_speed_hz = 1000000 \
}

bme280_handle_t bme280_create(const bme280_config_t *config);
esp_err_t bme280_delete(bme280_handle_t handle);
esp_err_t bme280_read_sensor(bme280_handle_t handle, 
                              float *temp, float *pressure, float *humidity);
```

**Implementation** (`bme280_spi.c`):

```c
struct bme280_dev_t {
    spi_device_handle_t spi;
    int cs_pin;
};

bme280_handle_t bme280_create(const bme280_config_t *config) {
    // Allocate handle
    bme280_handle_t handle = malloc(sizeof(struct bme280_dev_t));
    if (handle == NULL) return NULL;
    
    handle->cs_pin = config->cs_pin;
    
    // Configure SPI device
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = config->clock_speed_hz,
        .mode = 0,                      // SPI mode 0
        .spics_io_num = config->cs_pin, // CS pin
        .queue_size = 7,                // Transaction queue size
        .flags = 0,
    };
    
    // Add device to SPI bus
    esp_err_t ret = spi_bus_add_device(config->host, &dev_cfg, &handle->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device");
        free(handle);
        return NULL;
    }
    
    // Device initialization (read ID, configure, etc.)
    // ... (device-specific code) ...
    
    return handle;
}

esp_err_t bme280_read_sensor(bme280_handle_t handle, 
                              float *temp, float *pressure, float *humidity) {
    if (handle == NULL) return ESP_ERR_INVALID_ARG;
    
    // SPI transaction to read data
    uint8_t tx_data[1] = {0xF7};  // Read register command
    uint8_t rx_data[8];
    
    spi_transaction_t t = {
        .length = 8,               // Bits (not bytes!)
        .tx_buffer = tx_data,
        .rxlength = 8 * 8,         // Receive 8 bytes
        .rx_buffer = rx_data,
    };
    
    esp_err_t ret = spi_device_transmit(handle->spi, &t);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Parse received data
    // ... (device-specific) ...
    
    return ESP_OK;
}
```

### Multiple SPI Devices

**Key difference from I2C**: Each device needs separate CS pin.

```c
void app_main(void) {
    // Initialize SPI bus (once)
    spi_bus_config_t bus_cfg = {
        .miso_io_num = 12,
        .mosi_io_num = 13,
        .sclk_io_num = 14,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    
    // Create device 1 (CS = GPIO 10)
    bme280_config_t cfg1 = BME280_CONFIG_DEFAULT();
    cfg1.cs_pin = 10;
    bme280_handle_t sensor1 = bme280_create(&cfg1);
    
    // Create device 2 (CS = GPIO 11)
    bme280_config_t cfg2 = BME280_CONFIG_DEFAULT();
    cfg2.cs_pin = 11;
    bme280_handle_t sensor2 = bme280_create(&cfg2);
    
    // Create device 3 (CS = GPIO 15)
    bme280_config_t cfg3 = BME280_CONFIG_DEFAULT();
    cfg3.cs_pin = 15;
    bme280_handle_t sensor3 = bme280_create(&cfg3);
    
    // Read sensors independently
    float t1, p1, h1, t2, p2, h2, t3, p3, h3;
    bme280_read_sensor(sensor1, &t1, &p1, &h1);
    bme280_read_sensor(sensor2, &t2, &p2, &h2);
    bme280_read_sensor(sensor3, &t3, &p3, &h3);
}
```

**Important**: No "scanning" for SPI devices - you must know CS pins.

---

# Section D: UART Communication Fundamentals

## Understanding UART

**UART** (Universal Asynchronous Receiver/Transmitter):
- **Asynchronous**: No shared clock
- **Serial**: One bit at a time
- **Full-duplex**: Can send and receive simultaneously

### UART Signal Anatomy

```
Idle (High)
   │
   │  Start  Data bits (8)          Parity  Stop
   │   bit   │ │ │ │ │ │ │ │ │      bit    bit
   ▼   ▼     ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼       ▼      ▼
───┐   ┌─────────────────────────┐  ┌───┐ ┌─────
   └───┘                         └──┘   └─┘

   Idle → Start (0) → 8 data bits → Parity → Stop (1) → Idle
```

**Frame structure**:
1. **Idle**: Line stays HIGH
2. **Start bit**: Transition to LOW (signals data coming)
3. **Data bits**: 5-9 bits (usually 8)
4. **Parity bit**: Optional error checking
5. **Stop bit(s)**: Back to HIGH (1 or 2 bits)

**Common configurations**:
- 9600 8N1: 9600 baud, 8 data bits, No parity, 1 stop bit
- 115200 8N1: 115200 baud, 8 data bits, No parity, 1 stop bit

### UART on ESP32

ESP32-S3 has 3 UART ports:
- **UART0**: Usually for console/programming (USB)
- **UART1**: Available for general use
- **UART2**: Available for general use

**Wiring**:
```
ESP32 UART1          Device
TX (GPIO 17) ─────>  RX
RX (GPIO 18) <─────  TX
GND ──────────────── GND
```

**Note**: TX → RX crossover!

---

## UART Basic Setup

### Configuration

```c
#include "driver/uart.h"

#define UART_PORT UART_NUM_1
#define TX_PIN 17
#define RX_PIN 18
#define BUF_SIZE 1024

void uart_init(void) {
    // UART configuration
    uart_config_t uart_config = {
        .baud_rate = 115200,              // Speed
        .data_bits = UART_DATA_8_BITS,    // 8 data bits
        .parity = UART_PARITY_DISABLE,    // No parity
        .stop_bits = UART_STOP_BITS_1,    // 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  // No flow control
        .source_clk = UART_SCLK_APB,      // Clock source
    };
    
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    
    // Set UART pins (TX, RX, RTS, CTS)
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, TX_PIN, RX_PIN, 
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Install UART driver (with RX buffer)
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 
                                        BUF_SIZE * 2,  // RX buffer
                                        BUF_SIZE * 2,  // TX buffer
                                        0,             // Queue size
                                        NULL,          // Queue handle
                                        0));           // Interrupt flags
    
    ESP_LOGI(TAG, "UART initialized: %d baud, 8N1", 115200);
}
```

**Line-by-line**:

1. **baud_rate**: Bits per second
   - Must match device you're communicating with
   - Common: 9600, 115200, 460800

2. **data_bits**: Number of data bits per frame
   - Almost always 8 bits (1 byte)
   - Rarely: 5, 6, 7, or 9

3. **parity**: Error checking bit
   - DISABLE: No parity (most common)
   - EVEN/ODD: Single bit error detection

4. **stop_bits**: Frame termination
   - 1 bit (standard)
   - 2 bits (slower, more reliable)

5. **uart_driver_install()**:
   - Allocates RX/TX buffers in RAM
   - Sets up interrupt handlers
   - Creates internal queue for received data

### Basic Send/Receive

**Sending data**:

```c
void uart_send_string(const char *str) {
    // Write string to UART
    int len = strlen(str);
    int written = uart_write_bytes(UART_PORT, str, len);
    
    if (written != len) {
        ESP_LOGW(TAG, "Only wrote %d of %d bytes", written, len);
    }
    
    // Wait for transmission complete
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
}

void uart_send_bytes(const uint8_t *data, size_t len) {
    uart_write_bytes(UART_PORT, data, len);
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
}
```

**Receiving data**:

```c
int uart_receive_bytes(uint8_t *buf, size_t max_len, uint32_t timeout_ms) {
    // Read available data (non-blocking)
    int len = uart_read_bytes(UART_PORT, buf, max_len, 
                              pdMS_TO_TICKS(timeout_ms));
    return len;
}

// Example: Read until newline
int uart_read_line(char *line_buf, size_t max_len, uint32_t timeout_ms) {
    size_t pos = 0;
    uint64_t start_time = esp_timer_get_time() / 1000;
    
    while (pos < max_len - 1) {
        uint8_t c;
        int len = uart_read_bytes(UART_PORT, &c, 1, pdMS_TO_TICKS(10));
        
        if (len > 0) {
            if (c == '\n' || c == '\r') {
                line_buf[pos] = '\0';
                return pos;
            }
            line_buf[pos++] = c;
        }
        
        // Check timeout
        uint64_t elapsed = (esp_timer_get_time() / 1000) - start_time;
        if (elapsed > timeout_ms) {
            line_buf[pos] = '\0';
            return -1;  // Timeout
        }
    }
    
    line_buf[pos] = '\0';
    return pos;
}
```

---

# Section E: UART Protocol Design

## Why Protocols Matter

**Problem**: Raw UART is just bytes. How do you know:
- Where one message ends and another begins?
- If data was corrupted?
- What type of message it is?

**Solution**: Define a protocol!

## Protocol Design Principles

### 1. Framing: Message Boundaries

**Option A: Fixed delimiters**

```
START_BYTE | DATA | END_BYTE
   0x02    | ... |   0x03
```

Example:
```
0x02 0x41 0x42 0x43 0x03  → Message: "ABC"
```

**Option B: Length prefix**

```
LENGTH | DATA
 0x03  | 0x41 0x42 0x43  → 3 bytes: "ABC"
```

**Option C: Start + Length**

```
START | LENGTH | DATA
0x02  |  0x03  | 0x41 0x42 0x43
```

### 2. Error Detection

**Checksum** (simple):
```
DATA | CHECKSUM
...  | sum(bytes) & 0xFF
```

**CRC** (better):
```
DATA | CRC16
...  | crc16_calculate(DATA)
```

### 3. Message Types

**Command/Response**:
```
TYPE | PAYLOAD | CHECKSUM
0x01 |  ...    |   ...
```

Where TYPE might be:
- 0x01: Read temperature
- 0x02: Set configuration
- 0x03: Reset device
- 0x80: Response (success)
- 0xFF: Error

---

## Example Protocol: Simple Sensor Protocol

### Protocol Specification

```
Frame format:
┌──────┬────────┬────────┬──────────┬──────────┐
│ SOF  │ LENGTH │  TYPE  │ PAYLOAD  │   CRC8   │
├──────┼────────┼────────┼──────────┼──────────┤
│ 0xAA │ 1 byte │ 1 byte │ N bytes  │  1 byte  │
└──────┴────────┴────────┴──────────┴──────────┘

SOF (Start Of Frame): 0xAA
LENGTH: Number of bytes in TYPE + PAYLOAD + CRC8
TYPE: Message type (see below)
PAYLOAD: Message-specific data
CRC8: CRC-8 checksum of LENGTH + TYPE + PAYLOAD
```

**Message types**:
```c
#define MSG_TYPE_READ_TEMP      0x01
#define MSG_TYPE_SET_CONFIG     0x02
#define MSG_TYPE_RESPONSE       0x80
#define MSG_TYPE_ERROR          0xFF
```

### Implementation

**Protocol header** (`uart_protocol.h`):

```c
#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>
#include "esp_err.h"

// Protocol constants
#define PROTOCOL_SOF            0xAA
#define PROTOCOL_MAX_PAYLOAD    64

// Message types
#define MSG_TYPE_READ_TEMP      0x01
#define MSG_TYPE_SET_CONFIG     0x02
#define MSG_TYPE_RESPONSE       0x80
#define MSG_TYPE_ERROR          0xFF

// Message structure
typedef struct {
    uint8_t type;
    uint8_t payload_len;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
} uart_message_t;

/**
 * @brief Initialize UART protocol
 */
esp_err_t uart_protocol_init(int uart_num, int tx_pin, int rx_pin, int baud_rate);

/**
 * @brief Send a message
 * 
 * @param msg Message to send
 * @return ESP_OK on success
 */
esp_err_t uart_protocol_send(const uart_message_t *msg);

/**
 * @brief Receive a message (blocking with timeout)
 * 
 * @param msg Buffer to store received message
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t uart_protocol_receive(uart_message_t *msg, uint32_t timeout_ms);

#endif // UART_PROTOCOL_H
```

**Protocol implementation** (`uart_protocol.c`):

```c
#include "uart_protocol.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "uart_protocol";
static int g_uart_num = UART_NUM_1;

// CRC-8 lookup table (polynomial 0x07)
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    // ... (full table - 256 bytes)
    // Generate with: python -c "print(','.join([hex((i^(i<<1^i<<2^i<<3)&0xFF) for i in range(256)]))"
};

/**
 * @brief Calculate CRC-8
 * 
 * @param data Data buffer
 * @param len Data length
 * @return CRC-8 value
 */
static uint8_t crc8_calculate(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

esp_err_t uart_protocol_init(int uart_num, int tx_pin, int rx_pin, int baud_rate) {
    g_uart_num = uart_num;
    
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_pin, rx_pin, 
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(uart_num, 1024, 1024, 0, NULL, 0));
    
    ESP_LOGI(TAG, "UART protocol initialized on UART%d", uart_num);
    return ESP_OK;
}

esp_err_t uart_protocol_send(const uart_message_t *msg) {
    if (msg == NULL || msg->payload_len > PROTOCOL_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build frame
    uint8_t frame[PROTOCOL_MAX_PAYLOAD + 5];  // SOF + LEN + TYPE + PAYLOAD + CRC
    size_t frame_len = 0;
    
    // 1. Start of frame
    frame[frame_len++] = PROTOCOL_SOF;
    
    // 2. Length (TYPE + PAYLOAD + CRC)
    uint8_t length = 1 + msg->payload_len + 1;
    frame[frame_len++] = length;
    
    // 3. Type
    frame[frame_len++] = msg->type;
    
    // 4. Payload
    memcpy(&frame[frame_len], msg->payload, msg->payload_len);
    frame_len += msg->payload_len;
    
    // 5. CRC (over LENGTH + TYPE + PAYLOAD)
    uint8_t crc = crc8_calculate(&frame[1], length - 1);
    frame[frame_len++] = crc;
    
    // Send frame
    int written = uart_write_bytes(g_uart_num, frame, frame_len);
    if (written != frame_len) {
        ESP_LOGE(TAG, "Failed to write complete frame");
        return ESP_FAIL;
    }
    
    uart_wait_tx_done(g_uart_num, pdMS_TO_TICKS(100));
    
    ESP_LOGD(TAG, "Sent frame: type=0x%02X, len=%d", msg->type, msg->payload_len);
    return ESP_OK;
}

esp_err_t uart_protocol_receive(uart_message_t *msg, uint32_t timeout_ms) {
    if (msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t byte;
    uint64_t start_time = esp_timer_get_time() / 1000;
    
    // 1. Wait for Start Of Frame
    while (1) {
        if (uart_read_bytes(g_uart_num, &byte, 1, pdMS_TO_TICKS(10)) == 1) {
            if (byte == PROTOCOL_SOF) {
                break;  // Found SOF
            }
        }
        
        if ((esp_timer_get_time() / 1000 - start_time) > timeout_ms) {
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // 2. Read LENGTH
    uint8_t length;
    if (uart_read_bytes(g_uart_num, &length, 1, pdMS_TO_TICKS(100)) != 1) {
        ESP_LOGE(TAG, "Failed to read LENGTH");
        return ESP_FAIL;
    }
    
    if (length < 2 || length > PROTOCOL_MAX_PAYLOAD + 2) {
        ESP_LOGE(TAG, "Invalid LENGTH: %d", length);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 3. Read TYPE + PAYLOAD + CRC
    uint8_t data[PROTOCOL_MAX_PAYLOAD + 2];
    int read_len = uart_read_bytes(g_uart_num, data, length, pdMS_TO_TICKS(timeout_ms));
    if (read_len != length) {
        ESP_LOGE(TAG, "Failed to read frame data");
        return ESP_FAIL;
    }
    
    // 4. Verify CRC
    uint8_t received_crc = data[length - 1];
    uint8_t calculated_crc = crc8_calculate(&length, 1);  // Include length in CRC
    calculated_crc = crc8_calculate(data, length - 1);   // Then TYPE + PAYLOAD
    
    if (received_crc != calculated_crc) {
        ESP_LOGE(TAG, "CRC mismatch: received=0x%02X, calculated=0x%02X",
                 received_crc, calculated_crc);
        return ESP_ERR_INVALID_CRC;
    }
    
    // 5. Extract message
    msg->type = data[0];
    msg->payload_len = length - 2;  // Subtract TYPE and CRC
    memcpy(msg->payload, &data[1], msg->payload_len);
    
    ESP_LOGD(TAG, "Received frame: type=0x%02X, len=%d", msg->type, msg->payload_len);
    return ESP_OK;
}
```

### Usage Example

**Sensor side** (receives commands, sends data):

```c
void sensor_task(void *pvParameters) {
    uart_protocol_init(UART_NUM_1, 17, 18, 115200);
    
    while (1) {
        uart_message_t msg;
        
        // Wait for command
        if (uart_protocol_receive(&msg, 1000) == ESP_OK) {
            switch (msg.type) {
                case MSG_TYPE_READ_TEMP: {
                    // Read temperature
                    float temp = read_temperature();
                    
                    // Send response
                    uart_message_t response = {
                        .type = MSG_TYPE_RESPONSE,
                        .payload_len = 4
                    };
                    memcpy(response.payload, &temp, sizeof(float));
                    uart_protocol_send(&response);
                    break;
                }
                
                case MSG_TYPE_SET_CONFIG: {
                    // Handle configuration
                    // ... (parse msg.payload)
                    
                    // Send acknowledgment
                    uart_message_t ack = {
                        .type = MSG_TYPE_RESPONSE,
                        .payload_len = 1
                    };
                    ack.payload[0] = 0x00;  // Success
                    uart_protocol_send(&ack);
                    break;
                }
            }
        }
    }
}
```

**Controller side** (sends commands, receives data):

```c
float read_remote_temperature(void) {
    // Send read command
    uart_message_t cmd = {
        .type = MSG_TYPE_READ_TEMP,
        .payload_len = 0
    };
    
    if (uart_protocol_send(&cmd) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command");
        return -999.0f;
    }
    
    // Wait for response
    uart_message_t response;
    if (uart_protocol_receive(&response, 1000) != ESP_OK) {
        ESP_LOGE(TAG, "No response received");
        return -999.0f;
    }
    
    if (response.type != MSG_TYPE_RESPONSE || response.payload_len != 4) {
        ESP_LOGE(TAG, "Invalid response");
        return -999.0f;
    }
    
    // Extract temperature
    float temp;
    memcpy(&temp, response.payload, sizeof(float));
    
    ESP_LOGI(TAG, "Remote temperature: %.2f°C", temp);
    return temp;
}
```

---

## Advanced Protocol Features

### 1. Message IDs (Request/Response Matching)

```c
typedef struct {
    uint8_t id;         // Message ID (increments)
    uint8_t type;
    uint8_t payload_len;
    uint8_t payload[64];
} uart_message_v2_t;

// Send with ID
msg.id = next_id++;
uart_protocol_send(&msg);

// Response includes same ID
response.id = received_msg.id;
```

### 2. Multi-byte Data Encoding

**Big-endian** (network byte order):
```c
// Send 16-bit value
uint16_t value = 1234;
payload[0] = (value >> 8) & 0xFF;  // MSB first
payload[1] = value & 0xFF;         // LSB second

// Receive
uint16_t received = (payload[0] << 8) | payload[1];
```

**Little-endian**:
```c
// Send
payload[0] = value & 0xFF;         // LSB first
payload[1] = (value >> 8) & 0xFF;  // MSB second
```

**Float encoding**:
```c
// Send float
float temp = 25.5;
memcpy(payload, &temp, sizeof(float));

// Receive
float received_temp;
memcpy(&received_temp, payload, sizeof(float));
```

### 3. Error Handling

```c
#define ERROR_INVALID_CMD    0x01
#define ERROR_CRC_FAIL       0x02
#define ERROR_TIMEOUT        0x03

// Send error response
uart_message_t error_msg = {
    .type = MSG_TYPE_ERROR,
    .payload_len = 1
};
error_msg.payload[0] = ERROR_INVALID_CMD;
uart_protocol_send(&error_msg);
```

---

## Questions to Reinforce Learning

1. **Why use a device handle pattern instead of global variables?**
   <details>
   <summary>Answer</summary>
   - Supports multiple devices simultaneously
   - Each device has independent state
   - No global state pollution
   - Thread-safe (each handle independent)
   - Can create/destroy devices dynamically
   </details>

2. **In SPI, why does each device need a separate CS pin?**
   <details>
   <summary>Answer</summary>
   - CS determines which device is active
   - Multiple devices share data lines (MOSI/MISO)
   - Only device with CS LOW responds
   - Allows time-multiplexing one SPI bus
   </details>

3. **Why is CRC better than simple checksum?**
   <details>
   <summary>Answer</summary>
   - Detects burst errors (multiple bits)
   - Detects bit position swaps
   - Lower false-positive rate
   - Mathematical guarantee of error detection
   - Checksum can miss errors if they cancel out
   </details>

4. **What happens if UART devices use different baud rates?**
   <details>
   <summary>Answer</summary>
   - Timing mismatch
   - Bits sampled at wrong time
   - Garbage data received
   - Frame errors (start/stop bits wrong)
   - Must match exactly (within ~5% tolerance)
   </details>

5. **Why separate LENGTH and TYPE in the protocol?**
   <details>
   <summary>Answer</summary>
   - LENGTH: Receiver knows how many bytes to read
   - TYPE: Determines how to parse payload
   - Can add new message types without changing frame structure
   - Parser can handle unknown types gracefully
   </details>

---

## Summary

**Device Enumeration**:
- **I2C**: Scan addresses, verify manufacturer ID
- **SPI**: No scanning - must know CS pins
- **UART**: Device discovery via protocol commands

**Multi-Device Patterns**:
- Device handle with per-instance state
- Registry for managing multiple devices
- Auto-discovery and initialization

**Protocol Design**:
- Framing (delimiters, length)
- Error detection (checksum, CRC)
- Message types and structure
- Request/response matching

**Next Applications**:
- Implement device hot-plug detection
- Add firmware update via UART
- Build multi-sensor aggregation system
- Create debugging/diagnostic protocols

**Practice Exercise**:

Create a system with:
1. Three MCP9808 sensors on I2C (addresses 0x18, 0x19, 0x1A)
2. One BME280 on SPI
3. UART protocol to remote display
4. All data aggregated and sent via UART every second

This combines all concepts from this tutorial!
