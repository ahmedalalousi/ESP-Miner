# ESP-Miner System Architecture Guide

Comprehensive guide to understanding ESP-Miner's architecture, webapp interaction, configuration system, and hardware integration.

## Table of Contents
1. [System Overview](#system-overview)
2. [WebApp ↔ ESP32 Communication](#webapp--esp32-communication)
3. [Configuration System](#configuration-system)
4. [Hardware Support](#hardware-support)
5. [Adding New Hardware](#adding-new-hardware)
6. [Writing Drivers](#writing-drivers)

---

## System Overview

### High-Level Architecture

```
┌─────────────────────────────────────────────────┐
│           User's Browser                        │
│  ┌──────────────────────────────────┐           │
│  │  Angular Web Application         │           │
│  │  (TypeScript/HTML/CSS)          │           │
│  └────────────┬─────────────────────┘           │
└───────────────┼─────────────────────────────────┘
                │ HTTP/WebSocket
                ↓
┌─────────────────────────────────────────────────┐
│           ESP32-S3 Microcontroller              │
│  ┌──────────────────────────────────┐           │
│  │  HTTP Server (esp_http_server)   │           │
│  │  - Serves Web UI                 │           │
│  │  - REST API Endpoints            │           │
│  │  - WebSocket for real-time data │           │
│  └────────────┬─────────────────────┘           │
│               │                                  │
│  ┌────────────┴────────────────┐                │
│  │  Application Layer          │                │
│  │  - System Management        │                │
│  │  - Statistics Collection    │                │
│  │  - Configuration Management │                │
│  └────────────┬────────────────┘                │
│               │                                  │
│  ┌────────────┴────────────────┐                │
│  │  Hardware Abstraction       │                │
│  │  - ASIC Driver              │                │
│  │  - Power Management         │                │
│  │  - Thermal Control          │                │
│  │  - I2C/SPI/GPIO             │                │
│  └────────────┬────────────────┘                │
│               │                                  │
│  ┌────────────┴────────────────┐                │
│  │  Physical Hardware          │                │
│  │  - Mining ASIC              │                │
│  │  - Voltage Regulator        │                │
│  │  - Temperature Sensor       │                │
│  │  - Fan Controller           │                │
│  └─────────────────────────────┘                │
└──────────────────────────────────────────────────┘
```

---

## WebApp ↔ ESP32 Communication

### Communication Protocols

ESP-Miner uses **two methods** for communication:

#### 1. **HTTP REST API** - For Configuration and Control

Request/Response pattern for:
- Getting system information
- Updating settings
- Triggering actions

#### 2. **WebSocket** - For Real-Time Data

Bidirectional, continuous connection for:
- Live mining statistics
- Temperature monitoring
- Power consumption
- Hash rate updates

### HTTP REST API

#### Architecture

```
Browser                    ESP32-S3
   │                          │
   │  GET /api/system/info    │
   ├─────────────────────────>│
   │                          │ [Read system stats]
   │                          │
   │  ← JSON Response         │
   │<─────────────────────────┤
   │                          │
   │  POST /api/system/       │
   │       restart            │
   ├─────────────────────────>│
   │                          │ [Trigger restart]
   │                          │
   │  ← {"status": "ok"}      │
   │<─────────────────────────┤
```

#### Example API Endpoints

**Location**: `main/http_server/http_server.c`

```c
// Register REST endpoint
httpd_uri_t system_info_uri = {
    .uri = "/api/system/info",
    .method = HTTP_GET,
    .handler = system_info_handler,
    .user_ctx = NULL
};

httpd_register_uri_handler(server, &system_info_uri);

// Handler function
static esp_err_t system_info_handler(httpd_req_t *req) {
    // Create JSON response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "1.2.0");
    cJSON_AddNumberToObject(root, "uptime", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    
    // Send response
    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    // Cleanup
    free((void*)json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}
```

**In Angular (TypeScript)**:

```typescript
// Service: system.service.ts
getSystemInfo(): Observable<SystemInfo> {
  return this.http.get<SystemInfo>('/api/system/info');
}

// Component: home.component.ts
ngOnInit() {
  this.systemService.getSystemInfo().subscribe(
    info => {
      this.version = info.version;
      this.uptime = info.uptime;
      this.freeHeap = info.free_heap;
    }
  );
}
```

### WebSocket Communication

#### Architecture

```
Browser                    ESP32-S3
   │                          │
   │  WebSocket Connect       │
   ├─────────────────────────>│
   │  ws://192.168.1.65/ws    │
   │                          │
   │  ← Connected             │
   │<─────────────────────────┤
   │                          │
   │  ← Stats Update (JSON)   │
   │<─────────────────────────┤ [Every second]
   │  ← Stats Update (JSON)   │
   │<─────────────────────────┤
   │  ← Stats Update (JSON)   │
   │<─────────────────────────┤
   │                          │
   │  Command (JSON) →        │
   ├─────────────────────────>│
   │                          │
```

#### ESP32 WebSocket Server

**Location**: `main/http_server/http_server.c`

```c
// WebSocket handler
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake");
        return ESP_OK;
    }
    
    // Handle WebSocket frame
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    // Receive frame
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    
    // Send data to client
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "hashrate", get_hashrate());
    cJSON_AddNumberToObject(root, "temperature", get_temperature());
    
    const char *json_str = cJSON_PrintUnformatted(root);
    
    ws_pkt.payload = (uint8_t*)json_str;
    ws_pkt.len = strlen(json_str);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    httpd_ws_send_frame(req, &ws_pkt);
    
    free((void*)json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

// Periodic stats broadcast
void stats_task(void *pvParameters) {
    while(1) {
        // Broadcast to all connected clients
        broadcast_stats();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

#### Angular WebSocket Client

**Location**: `src/app/services/web-socket.service.ts`

```typescript
export class WebSocketService {
  private socket: WebSocket;
  private statsSubject = new Subject<Statistics>();
  
  connect() {
    this.socket = new WebSocket('ws://' + window.location.host + '/ws');
    
    this.socket.onmessage = (event) => {
      const data = JSON.parse(event.data);
      this.statsSubject.next(data);
    };
    
    this.socket.onerror = (error) => {
      console.error('WebSocket error:', error);
    };
  }
  
  getStats(): Observable<Statistics> {
    return this.statsSubject.asObservable();
  }
  
  sendCommand(command: any) {
    this.socket.send(JSON.stringify(command));
  }
}
```

---

## Configuration System

### Configuration Storage (NVS)

**NVS** (Non-Volatile Storage) is ESP32's key-value storage:

```
Flash Partition "nvs" @ 0x9000
┌────────────────────────────┐
│ Key: "wifissid"            │
│ Value: "MyNetwork"         │
├────────────────────────────┤
│ Key: "wifipass"            │
│ Value: "MyPassword"        │
├────────────────────────────┤
│ Key: "asicfrequency"       │
│ Value: 485                 │
├────────────────────────────┤
│ Key: "asicvoltage"         │
│ Value: 1200                │
└────────────────────────────┘
```

### Reading Configuration

**Location**: `main/nvs_config.c`

```c
#include "nvs_flash.h"
#include "nvs.h"

// Open NVS
nvs_handle_t nvs_handle;
esp_err_t err = nvs_open("main", NVS_READONLY, &nvs_handle);

// Read string
char wifi_ssid[32];
size_t len = sizeof(wifi_ssid);
nvs_get_str(nvs_handle, "wifissid", wifi_ssid, &len);

// Read integer
uint16_t asic_freq;
nvs_get_u16(nvs_handle, "asicfrequency", &asic_freq);

// Close NVS
nvs_close(nvs_handle);
```

### Writing Configuration

```c
// Open NVS for writing
nvs_handle_t nvs_handle;
nvs_open("main", NVS_READWRITE, &nvs_handle);

// Write string
nvs_set_str(nvs_handle, "wifissid", "NewNetwork");

// Write integer
nvs_set_u16(nvs_handle, "asicfrequency", 500);

// Commit changes
nvs_commit(nvs_handle);

// Close
nvs_close(nvs_handle);
```

### Configuration from Web UI

**Flow**:
```
1. User changes setting in web UI
   ↓
2. Angular sends POST to /api/settings
   ↓
3. ESP32 receives JSON
   ↓
4. Parse JSON and validate
   ↓
5. Write to NVS
   ↓
6. Apply changes to running system
   ↓
7. Send success response
```

**Example**:

```c
// POST /api/settings
static esp_err_t settings_handler(httpd_req_t *req) {
    // Read POST data
    char content[512];
    httpd_req_recv(req, content, req->content_len);
    
    // Parse JSON
    cJSON *root = cJSON_Parse(content);
    
    // Get values
    cJSON *freq = cJSON_GetObjectItem(root, "frequency");
    cJSON *voltage = cJSON_GetObjectItem(root, "voltage");
    
    // Write to NVS
    nvs_handle_t nvs;
    nvs_open("main", NVS_READWRITE, &nvs);
    nvs_set_u16(nvs, "asicfrequency", freq->valueint);
    nvs_set_u16(nvs, "asicvoltage", voltage->valueint);
    nvs_commit(nvs);
    nvs_close(nvs);
    
    // Apply changes
    asic_set_frequency(freq->valueint);
    power_set_voltage(voltage->valueint);
    
    // Response
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    cJSON_Delete(root);
    
    return ESP_OK;
}
```

---

## Hardware Support

### Device Configuration

**Location**: `main/device_config.c`

ESP-Miner supports multiple hardware variants through a configuration system:

```c
typedef struct {
    char board_version[32];    // "401", "601", etc.
    char device_model[32];     // "max", "ultra", "supra"
    char asic_model[32];       // "BM1397", "BM1366", "BM1368"
    uint8_t asic_count;        // Number of ASICs
    uint16_t asic_difficulty;  // Mining difficulty
    // ... hardware-specific parameters
} device_config_t;
```

### Hardware Detection

ESP-Miner uses **both**:

1. **Compile-time configuration** - `config-xxx.cvs` files
2. **Runtime detection** - I2C device scanning

```c
void detect_hardware(device_config_t *config) {
    // Try to detect voltage regulator
    if (i2c_probe_device(I2C_NUM_0, TPS546_ADDR)) {
        config->has_tps546 = true;
        ESP_LOGI(TAG, "Detected TPS546 voltage regulator");
    }
    
    // Try to detect temperature sensor
    if (i2c_probe_device(I2C_NUM_0, EMC2101_ADDR)) {
        config->has_emc2101 = true;
        ESP_LOGI(TAG, "Detected EMC2101 temp sensor");
    }
    
    // Detect ASIC type by communication test
    if (asic_detect_bm1368()) {
        strcpy(config->asic_model, "BM1368");
    }
}
```

### Hardware Abstraction

**Power management example**:

```
┌─────────────────────────────────┐
│  Application Layer              │
│  power_set_voltage(1200)        │
└────────────┬────────────────────┘
             │
┌────────────┴────────────────────┐
│  HAL (Hardware Abstraction)     │
│  vcore_set_voltage()            │
└────────────┬────────────────────┘
             │
         ┌───┴────┬──────────┬────────┐
         │        │          │        │
    ┌────┴───┐ ┌─┴──────┐ ┌─┴─────┐ │
    │ TPS546 │ │ DS4432U│ │ Custom│ │
    │ Driver │ │ Driver │ │ Driver│ │
    └────────┘ └────────┘ └───────┘ │
```

**Implementation**:

```c
// HAL function
esp_err_t vcore_set_voltage(uint16_t millivolts) {
    if (config.has_tps546) {
        return tps546_set_voltage(millivolts);
    } else if (config.has_ds4432u) {
        return ds4432u_set_voltage(millivolts);
    } else {
        ESP_LOGE(TAG, "No voltage regulator configured");
        return ESP_ERR_NOT_SUPPORTED;
    }
}

// Driver-specific implementation
esp_err_t tps546_set_voltage(uint16_t millivolts) {
    // Calculate register value
    uint16_t reg_value = voltage_to_tps546_register(millivolts);
    
    // Write to I2C device
    return i2c_write_reg(TPS546_ADDR, TPS546_VOUT_CMD, reg_value);
}
```

---

## Adding New Hardware

### Step-by-Step: Adding a New Component

Let's add support for a **hypothetical ADS1118 ADC** for voltage monitoring.

#### Step 1: Create Driver Component

```bash
cd components
mkdir ads1118
cd ads1118
```

Create `CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "ads1118.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver" "esp_timer"
)
```

Create `include/ads1118.h`:
```c
#ifndef ADS1118_H
#define ADS1118_H

#include "esp_err.h"
#include <stdint.h>

// Device I2C address
#define ADS1118_ADDR 0x48

// Initialize the ADS1118
esp_err_t ads1118_init(void);

// Read voltage from channel
esp_err_t ads1118_read_voltage(uint8_t channel, float *voltage);

// Check if device is present
bool ads1118_is_present(void);

#endif
```

Create `ads1118.c`:
```c
#include "ads1118.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "ads1118";

// Register definitions
#define ADS1118_REG_CONFIG 0x01
#define ADS1118_REG_CONVERSION 0x00

esp_err_t ads1118_init(void) {
    ESP_LOGI(TAG, "Initializing ADS1118");
    
    // Check if device is present
    if (!ads1118_is_present()) {
        ESP_LOGE(TAG, "ADS1118 not found on I2C bus");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Configure device
    uint16_t config = 0x8583; // Default config
    esp_err_t ret = i2c_master_write_to_device(
        I2C_NUM_0,
        ADS1118_ADDR,
        (uint8_t*)&config,
        2,
        pdMS_TO_TICKS(1000)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADS1118");
        return ret;
    }
    
    ESP_LOGI(TAG, "ADS1118 initialized successfully");
    return ESP_OK;
}

esp_err_t ads1118_read_voltage(uint8_t channel, float *voltage) {
    if (channel > 3) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Read conversion result
    uint8_t data[2];
    esp_err_t ret = i2c_master_read_from_device(
        I2C_NUM_0,
        ADS1118_ADDR,
        data,
        2,
        pdMS_TO_TICKS(1000)
    );
    
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Convert to voltage
    int16_t raw = (data[0] << 8) | data[1];
    *voltage = (float)raw * 4.096f / 32768.0f;
    
    return ESP_OK;
}

bool ads1118_is_present(void) {
    uint8_t dummy;
    esp_err_t ret = i2c_master_read_from_device(
        I2C_NUM_0,
        ADS1118_ADDR,
        &dummy,
        1,
        pdMS_TO_TICKS(100)
    );
    
    return (ret == ESP_OK);
}
```

#### Step 2: Integrate with Configuration System

Edit `main/device_config.h`:
```c
typedef struct {
    // ... existing fields
    bool has_ads1118;  // Add new hardware flag
} device_config_t;
```

Edit `main/device_config.c`:
```c
#include "ads1118.h"

void detect_hardware(device_config_t *config) {
    // ... existing detection
    
    // Detect ADS1118
    if (ads1118_is_present()) {
        config->has_ads1118 = true;
        ESP_LOGI(TAG, "Detected ADS1118 ADC");
        ads1118_init();
    }
}
```

#### Step 3: Add Configuration Options

Create `components/ads1118/Kconfig`:
```
menu "ADS1118 ADC Configuration"

config ADS1118_ENABLE
    bool "Enable ADS1118 ADC"
    default n
    help
        Enable support for ADS1118 ADC

config ADS1118_I2C_ADDR
    hex "ADS1118 I2C Address"
    depends on ADS1118_ENABLE
    default 0x48
    help
        I2C address of ADS1118

endmenu
```

#### Step 4: Use in Application

Edit `components/voltage_monitor/voltage_monitor.c`:
```c
#include "ads1118.h"

void voltage_monitor_task(void *pvParameters) {
    while(1) {
        if (GLOBAL_STATE->DEVICE_CONFIG.has_ads1118) {
            float voltage;
            if (ads1118_read_voltage(0, &voltage) == ESP_OK) {
                ESP_LOGI(TAG, "ASIC Voltage: %.3fV", voltage);
                // Store in global state
                GLOBAL_STATE->voltage = voltage;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

#### Step 5: Expose via API

Edit `main/http_server/http_server.c`:
```c
static esp_err_t voltage_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    
    if (GLOBAL_STATE->DEVICE_CONFIG.has_ads1118) {
        cJSON_AddNumberToObject(root, "voltage", GLOBAL_STATE->voltage);
        cJSON_AddBoolToObject(root, "supported", true);
    } else {
        cJSON_AddBoolToObject(root, "supported", false);
    }
    
    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void*)json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}
```

#### Step 6: Add to Web UI

Edit `src/app/components/home/home.component.ts`:
```typescript
export class HomeComponent {
  voltage: number = 0;
  voltageSupported: boolean = false;
  
  ngOnInit() {
    this.systemService.getVoltage().subscribe(data => {
      this.voltageSupported = data.supported;
      if (data.supported) {
        this.voltage = data.voltage;
      }
    });
  }
}
```

Edit `src/app/components/home/home.component.html`:
```html
<div *ngIf="voltageSupported" class="voltage-display">
  <h3>ASIC Voltage</h3>
  <p>{{voltage | number:'1.3-3'}} V</p>
</div>
```

---

## Writing Drivers

### Driver Structure

**Every driver should follow this pattern**:

```c
// 1. Initialization
esp_err_t driver_init(void);

// 2. Core functionality
esp_err_t driver_read(uint8_t *data, size_t len);
esp_err_t driver_write(const uint8_t *data, size_t len);

// 3. Configuration
esp_err_t driver_set_mode(uint8_t mode);
esp_err_t driver_get_status(driver_status_t *status);

// 4. Cleanup
esp_err_t driver_deinit(void);

// 5. Detection
bool driver_is_present(void);
```

### I2C Driver Example

```c
#include "driver/i2c.h"

#define DEVICE_ADDR 0x50
#define DEVICE_REG_STATUS 0x00
#define DEVICE_REG_CONFIG 0x01

// Write to register
esp_err_t device_write_reg(uint8_t reg, uint8_t value) {
    uint8_t write_buf[2] = {reg, value};
    
    return i2c_master_write_to_device(
        I2C_NUM_0,
        DEVICE_ADDR,
        write_buf,
        2,
        pdMS_TO_TICKS(1000)
    );
}

// Read from register
esp_err_t device_read_reg(uint8_t reg, uint8_t *value) {
    esp_err_t ret;
    
    // Write register address
    ret = i2c_master_write_to_device(
        I2C_NUM_0,
        DEVICE_ADDR,
        &reg,
        1,
        pdMS_TO_TICKS(1000)
    );
    
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Read value
    return i2c_master_read_from_device(
        I2C_NUM_0,
        DEVICE_ADDR,
        value,
        1,
        pdMS_TO_TICKS(1000)
    );
}
```

### SPI Driver Example

```c
#include "driver/spi_master.h"

spi_device_handle_t spi;

esp_err_t spi_driver_init(void) {
    // Bus configuration
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_NUM_13,
        .miso_io_num = GPIO_NUM_12,
        .sclk_io_num = GPIO_NUM_14,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    
    // Device configuration
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1*1000*1000,  // 1 MHz
        .mode = 0,
        .spics_io_num = GPIO_NUM_15,
        .queue_size = 7,
    };
    
    // Initialize SPI bus
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    
    // Attach device to bus
    return spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
}

esp_err_t spi_driver_write_read(const uint8_t *tx_data, uint8_t *rx_data, size_t len) {
    spi_transaction_t t = {
        .length = len * 8,  // Length in bits
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    
    return spi_device_transmit(spi, &t);
}
```

### GPIO Driver Example

```c
#include "driver/gpio.h"

#define OUTPUT_PIN GPIO_NUM_5
#define INPUT_PIN  GPIO_NUM_4

esp_err_t gpio_driver_init(void) {
    // Configure output
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << OUTPUT_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);
    
    // Configure input
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << INPUT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_conf);
    
    return ESP_OK;
}

void gpio_set_output(bool level) {
    gpio_set_level(OUTPUT_PIN, level ? 1 : 0);
}

bool gpio_read_input(void) {
    return gpio_get_level(INPUT_PIN);
}
```

---

## Summary

### Key Takeaways

1. **WebApp ↔ ESP32 Communication**:
   - REST API for configuration
   - WebSocket for real-time data
   - JSON for data exchange

2. **Configuration System**:
   - NVS for persistent storage
   - Key-value pairs
   - Accessible via web UI

3. **Hardware Support**:
   - Device configuration structure
   - Runtime hardware detection
   - Hardware abstraction layer

4. **Adding New Hardware**:
   - Create driver component
   - Integrate with config system
   - Add menuconfig options
   - Expose via API
   - Add to web UI

5. **Writing Drivers**:
   - Follow standard structure
   - Use ESP-IDF driver APIs
   - Implement detection
   - Add error handling

---

**Further Reading**:
- ESP-IDF Component System: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/build-system.html
- HTTP Server: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/protocols/esp_http_server.html
- I2C Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html
- SPI Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/spi_master.html
