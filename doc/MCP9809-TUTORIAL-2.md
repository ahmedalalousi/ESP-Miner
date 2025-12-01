# MCP9808 Temperature Sensor Tutorial - Part 2: Creating the Driver

Building the I2C driver for MCP9808 from scratch.

## Project Structure

We'll create a new component in the ESP-Miner project:

```
ESP-Miner/
├── components/
│   └── mcp9808/              # New component (we create this)
│       ├── CMakeLists.txt    # Build configuration
│       ├── include/
│       │   └── mcp9808.h     # Public header (API)
│       └── mcp9808.c         # Implementation
└── main/
    └── main.c                # We'll modify this to test
```

---

## Step 1: Create Component Directory

Open your terminal:

```bash
cd ~/Work/ESP-Miner/components

# Create the component directory
mkdir mcp9808
cd mcp9808

# Create subdirectories
mkdir include
```

---

## Step 2: Create CMakeLists.txt

This tells the build system about our component.

Create `components/mcp9808/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "mcp9808.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver" "esp_timer"
)
```

**Explanation**:
- `SRCS`: Source files to compile
- `INCLUDE_DIRS`: Where header files are
- `REQUIRES`: Dependencies (we need I2C driver and timer)

---

## Step 3: Create the Header File

Create `components/mcp9808/include/mcp9808.h`:

```c
#ifndef MCP9808_H
#define MCP9808_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief MCP9808 I2C address (default)
 * 
 * Can be 0x18-0x1F depending on A0, A1, A2 pin configuration
 */
#define MCP9808_I2C_ADDR_DEFAULT    0x18

/**
 * @brief MCP9808 Register addresses
 */
#define MCP9808_REG_CONFIG          0x01    // Configuration register
#define MCP9808_REG_TEMP_UPPER      0x02    // Upper temperature limit
#define MCP9808_REG_TEMP_LOWER      0x03    // Lower temperature limit
#define MCP9808_REG_TEMP_CRIT       0x04    // Critical temperature
#define MCP9808_REG_TEMP_AMBIENT    0x05    // Ambient temperature
#define MCP9808_REG_MANUFACTURER_ID 0x06    // Manufacturer ID
#define MCP9808_REG_DEVICE_ID       0x07    // Device ID & Revision
#define MCP9808_REG_RESOLUTION      0x08    // Resolution

/**
 * @brief Expected manufacturer ID
 */
#define MCP9808_MANUFACTURER_ID     0x0054

/**
 * @brief Expected device ID (upper byte)
 */
#define MCP9808_DEVICE_ID           0x04

/**
 * @brief Initialize the MCP9808 sensor
 * 
 * This function:
 * - Checks if the device is present on I2C bus
 * - Verifies manufacturer and device IDs
 * - Configures the sensor with default settings
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_NOT_FOUND: Device not found on I2C bus
 *     - ESP_ERR_INVALID_RESPONSE: Wrong manufacturer/device ID
 *     - ESP_FAIL: I2C communication failed
 */
esp_err_t mcp9808_init(void);

/**
 * @brief Check if MCP9808 is present on I2C bus
 * 
 * @return true if device responds, false otherwise
 */
bool mcp9808_is_present(void);

/**
 * @brief Read temperature from MCP9808
 * 
 * Reads the ambient temperature register and converts
 * the raw value to degrees Celsius.
 * 
 * @param[out] temperature Pointer to store temperature in °C
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: temperature pointer is NULL
 *     - ESP_FAIL: I2C communication failed
 */
esp_err_t mcp9808_read_temperature(float *temperature);

/**
 * @brief Read manufacturer ID
 * 
 * Should return 0x0054 for genuine MCP9808
 * 
 * @param[out] manufacturer_id Pointer to store manufacturer ID
 * 
 * @return ESP_OK on success, ESP_FAIL on I2C error
 */
esp_err_t mcp9808_read_manufacturer_id(uint16_t *manufacturer_id);

/**
 * @brief Read device ID
 * 
 * Upper byte should be 0x04 for MCP9808
 * Lower byte contains revision number
 * 
 * @param[out] device_id Pointer to store device ID
 * 
 * @return ESP_OK on success, ESP_FAIL on I2C error
 */
esp_err_t mcp9808_read_device_id(uint16_t *device_id);

#endif // MCP9808_H
```

**What we've defined**:
- I2C address and register addresses (from datasheet)
- Function declarations (the API)
- Detailed documentation comments

---

## Step 4: Implement the Driver

Create `components/mcp9808/mcp9808.c`:

```c
#include "mcp9808.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

// Tag for logging
static const char *TAG = "mcp9808";

// I2C port to use (ESP-Miner uses I2C_NUM_0)
#define I2C_MASTER_NUM I2C_NUM_0

// I2C timeout
#define I2C_TIMEOUT_MS 1000

/**
 * @brief Write to MCP9808 register
 * 
 * @param reg_addr Register address
 * @param data Data to write (16-bit, big-endian)
 * @return ESP_OK on success
 */
static esp_err_t mcp9808_write_register(uint8_t reg_addr, uint16_t data) {
    uint8_t write_buf[3];
    write_buf[0] = reg_addr;
    write_buf[1] = (data >> 8) & 0xFF;  // Upper byte
    write_buf[2] = data & 0xFF;          // Lower byte
    
    esp_err_t ret = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        MCP9808_I2C_ADDR_DEFAULT,
        write_buf,
        3,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02X: %s", 
                 reg_addr, esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Read from MCP9808 register
 * 
 * @param reg_addr Register address
 * @param data Pointer to store data (16-bit, big-endian)
 * @return ESP_OK on success
 */
static esp_err_t mcp9808_read_register(uint8_t reg_addr, uint16_t *data) {
    uint8_t read_buf[2];
    
    // Write register address, then read
    esp_err_t ret = i2c_master_write_read_device(
        I2C_MASTER_NUM,
        MCP9808_I2C_ADDR_DEFAULT,
        &reg_addr,
        1,
        read_buf,
        2,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", 
                 reg_addr, esp_err_to_name(ret));
        return ret;
    }
    
    // Combine bytes (big-endian)
    *data = (read_buf[0] << 8) | read_buf[1];
    
    return ESP_OK;
}

bool mcp9808_is_present(void) {
    uint8_t dummy;
    esp_err_t ret = i2c_master_read_from_device(
        I2C_MASTER_NUM,
        MCP9808_I2C_ADDR_DEFAULT,
        &dummy,
        1,
        pdMS_TO_TICKS(100)
    );
    
    return (ret == ESP_OK);
}

esp_err_t mcp9808_read_manufacturer_id(uint16_t *manufacturer_id) {
    if (manufacturer_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return mcp9808_read_register(MCP9808_REG_MANUFACTURER_ID, manufacturer_id);
}

esp_err_t mcp9808_read_device_id(uint16_t *device_id) {
    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return mcp9808_read_register(MCP9808_REG_DEVICE_ID, device_id);
}

esp_err_t mcp9808_init(void) {
    ESP_LOGI(TAG, "Initializing MCP9808 temperature sensor");
    
    // Check if device is present
    if (!mcp9808_is_present()) {
        ESP_LOGE(TAG, "MCP9808 not found on I2C bus at address 0x%02X", 
                 MCP9808_I2C_ADDR_DEFAULT);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "MCP9808 detected on I2C bus");
    
    // Read and verify manufacturer ID
    uint16_t mfg_id;
    esp_err_t ret = mcp9808_read_manufacturer_id(&mfg_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read manufacturer ID");
        return ret;
    }
    
    if (mfg_id != MCP9808_MANUFACTURER_ID) {
        ESP_LOGE(TAG, "Invalid manufacturer ID: 0x%04X (expected 0x%04X)", 
                 mfg_id, MCP9808_MANUFACTURER_ID);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGI(TAG, "Manufacturer ID verified: 0x%04X", mfg_id);
    
    // Read and verify device ID
    uint16_t dev_id;
    ret = mcp9808_read_device_id(&dev_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID");
        return ret;
    }
    
    uint8_t device_id_upper = (dev_id >> 8) & 0xFF;
    uint8_t revision = dev_id & 0xFF;
    
    if (device_id_upper != MCP9808_DEVICE_ID) {
        ESP_LOGE(TAG, "Invalid device ID: 0x%02X (expected 0x%02X)", 
                 device_id_upper, MCP9808_DEVICE_ID);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGI(TAG, "Device ID verified: 0x%02X, Revision: 0x%02X", 
             device_id_upper, revision);
    
    // Set configuration to default (continuous conversion, full resolution)
    // Configuration register: 0x0000 (all defaults)
    ret = mcp9808_write_register(MCP9808_REG_CONFIG, 0x0000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure device");
        return ret;
    }
    
    ESP_LOGI(TAG, "MCP9808 initialized successfully");
    
    return ESP_OK;
}

esp_err_t mcp9808_read_temperature(float *temperature) {
    if (temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint16_t raw_temp;
    esp_err_t ret = mcp9808_read_register(MCP9808_REG_TEMP_AMBIENT, &raw_temp);
    
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Clear flag bits (upper 3 bits)
    raw_temp = raw_temp & 0x1FFF;
    
    // Check if temperature is negative (bit 12)
    if (raw_temp & 0x1000) {
        // Negative temperature
        raw_temp = raw_temp & 0x0FFF;  // Clear sign bit
        *temperature = -(float)raw_temp * 0.0625f;
    } else {
        // Positive temperature
        *temperature = (float)raw_temp * 0.0625f;
    }
    
    ESP_LOGD(TAG, "Temperature: %.4f°C (raw: 0x%04X)", *temperature, raw_temp);
    
    return ESP_OK;
}
```

**What this code does**:

1. **Helper Functions**:
   - `mcp9808_write_register()`: Writes 16-bit value to register
   - `mcp9808_read_register()`: Reads 16-bit value from register

2. **Initialization** (`mcp9808_init()`):
   - Checks device presence
   - Verifies manufacturer ID (0x0054)
   - Verifies device ID (0x04)
   - Configures sensor

3. **Temperature Reading** (`mcp9808_read_temperature()`):
   - Reads 16-bit temperature register
   - Converts to float (°C)
   - Handles negative temperatures

---

## Step 5: Test the Driver

Now let's modify `main/main.c` to test our driver.

**First, tell the build system we need our component**:

Edit `main/CMakeLists.txt` and add `mcp9808` to REQUIRES:

```cmake
idf_component_register(
    SRCS "main.c" "other.c" ...
    INCLUDE_DIRS "."
    REQUIRES 
        "driver"
        "esp_wifi"
        "mcp9808"    # Add this line
        # ... other dependencies
)
```

**Now modify `main/main.c`**:

Add to the includes section:

```c
#include "mcp9808.h"
```

Add a test function:

```c
void test_mcp9808(void) {
    ESP_LOGI("main", "=== MCP9808 Temperature Sensor Test ===");
    
    // Initialize the sensor
    esp_err_t ret = mcp9808_init();
    if (ret != ESP_OK) {
        ESP_LOGE("main", "Failed to initialize MCP9808: %s", esp_err_to_name(ret));
        return;
    }
    
    // Read temperature 10 times
    for (int i = 0; i < 10; i++) {
        float temperature;
        ret = mcp9808_read_temperature(&temperature);
        
        if (ret == ESP_OK) {
            ESP_LOGI("main", "Reading %d: Temperature = %.2f°C", i+1, temperature);
        } else {
            ESP_LOGE("main", "Failed to read temperature: %s", esp_err_to_name(ret));
        }
        
        // Wait 1 second between readings
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

In your `app_main()` function, add the test:

```c
void app_main(void) {
    // ... existing initialization code ...
    
    // After I2C initialization, add:
    test_mcp9808();
    
    // ... rest of your code ...
}
```

---

## Step 6: Build and Flash

```bash
cd ~/Work/ESP-Miner

# Source ESP-IDF environment
cd ~/esp/esp-idf && . ./export.sh && cd ~/Work/ESP-Miner

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/cu.usbmodem141201 flash monitor
```

---

## Expected Output

You should see in the serial monitor:

```
I (1234) main: === MCP9808 Temperature Sensor Test ===
I (1235) mcp9808: Initializing MCP9808 temperature sensor
I (1240) mcp9808: MCP9808 detected on I2C bus
I (1245) mcp9808: Manufacturer ID verified: 0x0054
I (1250) mcp9808: Device ID verified: 0x04, Revision: 0x00
I (1255) mcp9808: MCP9808 initialized successfully
I (1260) main: Reading 1: Temperature = 24.56°C
I (2265) main: Reading 2: Temperature = 24.50°C
I (3270) main: Reading 3: Temperature = 24.50°C
I (4275) main: Reading 4: Temperature = 24.56°C
I (5280) main: Reading 5: Temperature = 24.50°C
I (6285) main: Reading 6: Temperature = 24.50°C
I (7290) main: Reading 7: Temperature = 24.56°C
I (8295) main: Reading 8: Temperature = 24.50°C
I (9300) main: Reading 9: Temperature = 24.56°C
I (10305) main: Reading 10: Temperature = 24.50°C
```

---

## Troubleshooting

### "MCP9808 not found on I2C bus"

**Check**:
1. Wiring connections (especially SDA/SCL)
2. Power to sensor (VCC connected?)
3. I2C pull-up resistors present
4. Correct I2C address (0x18 default)

**Debug**:
```c
// Add I2C scanner before mcp9808_init()
void i2c_scanner(void) {
    ESP_LOGI("main", "Scanning I2C bus...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        esp_err_t ret = i2c_master_read_from_device(
            I2C_NUM_0, addr, &dummy, 1, pdMS_TO_TICKS(100)
        );
        if (ret == ESP_OK) {
            ESP_LOGI("main", "  Found device at 0x%02X", addr);
        }
    }
}
```

### "Invalid manufacturer ID"

**Possible causes**:
- Wrong sensor connected
- Fake/counterfeit MCP9808
- I2C communication errors

### Temperature reads as 0.00°C or -0.00°C

- Check if sensor is powered
- Verify I2C communication is working
- Check register address is correct (0x05)

---

## Understanding the Code

### Why Two Helper Functions?

```c
static esp_err_t mcp9808_write_register(...)
static esp_err_t mcp9808_read_register(...)
```

- **static**: Only visible within this file (encapsulation)
- **Reusable**: All register access uses these functions
- **Error handling**: Centralized logging and error checking

### Big-Endian Byte Order

MCP9808 uses big-endian (most significant byte first):

```c
// Reading 0x1234 from sensor:
read_buf[0] = 0x12  // Upper byte
read_buf[1] = 0x34  // Lower byte

// Combine:
value = (0x12 << 8) | 0x34  // = 0x1234
```

### Temperature Conversion

From datasheet, each LSB = 0.0625°C:

```c
// Example: raw_temp = 0x0191 = 401
temperature = 401 × 0.0625 = 25.0625°C
```

---

## Next Steps

In **Part 3**, we'll:
1. Create a FreeRTOS task for continuous monitoring
2. Store data in GLOBAL_STATE
3. Create HTTP API endpoint
4. Test reading temperature from web browser

---

## Questions to Reinforce Learning

1. Why do we verify manufacturer and device IDs during init?
2. What would happen if we used little-endian instead of big-endian?
3. Why use `static` for helper functions?
4. How would you modify the code to support multiple MCP9808 sensors?

---

**Ready for Part 3?** Let me know when you've successfully tested the driver!
