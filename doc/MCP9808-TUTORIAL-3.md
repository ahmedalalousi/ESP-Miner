# MCP9808 Temperature Sensor Tutorial - Part 3: Creating the Web Server

Building a standalone ESP32 project with HTTP server, REST API, and web interface foundation.

## Project Overview

We're creating a **standalone temperature monitoring system**:

```
┌─────────────────────────────────────────────────────┐
│            Standalone ESP32 Project                  │
├─────────────────────────────────────────────────────┤
│                                                      │
│  Hardware Layer:                                     │
│  ┌──────────┐    I2C    ┌───────────────┐          │
│  │ MCP9808  │<──────────│ ESP32-S3      │          │
│  │  Sensor  │           │               │          │
│  └──────────┘           │ - WiFi AP     │          │
│                         │ - HTTP Server │          │
│                         │ - REST API    │          │
│                         └───────────────┘          │
│                                │                    │
│                         WiFi   │                    │
│                                │                    │
│  ┌─────────────────────────────▼──────────────┐   │
│  │         Web Browser                         │   │
│  │  ┌──────────────────────────────────────┐  │   │
│  │  │  Angular Web Application             │  │   │
│  │  │  - Temperature Gauge                 │  │   │
│  │  │  - History Chart                     │  │   │
│  │  │  - Control Buttons                   │  │   │
│  │  └──────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────┘   │
│                                                      │
└─────────────────────────────────────────────────────┘
```

**What we'll build in Part 3**:
1. Complete project structure from scratch
2. WiFi Access Point
3. HTTP web server
4. REST API endpoints
5. Basic HTML page (Angular comes in Part 4)
6. FreeRTOS task for temperature monitoring

---

## Step 1: Create Project Structure

Open terminal and create the project:

```bash
cd ~/Work

# Create project directory
mkdir temperature-monitor
cd temperature-monitor

# Create ESP-IDF project structure
mkdir -p main components/mcp9808/include www

# Create main files
touch main/CMakeLists.txt
touch main/main.c
touch main/Kconfig.projbuild

# Create component files (we'll copy from Part 2)
touch components/mcp9808/CMakeLists.txt
touch components/mcp9808/mcp9808.c
touch components/mcp9808/include/mcp9808.h

# Create root CMakeLists.txt
touch CMakeLists.txt

# Create sdkconfig.defaults
touch sdkconfig.defaults
```

Your structure should look like:

```
temperature-monitor/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   └── main.c
├── components/
│   └── mcp9808/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── mcp9808.h
│       └── mcp9808.c
└── www/
    └── (web files will go here in Part 4)
```

---

## Step 2: Create Root CMakeLists.txt

Edit `temperature-monitor/CMakeLists.txt`:

```cmake
# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# ESP-IDF uses this file to find the project root
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(temperature-monitor)
```

---

## Step 3: Copy MCP9808 Driver (from Part 2)

Copy your driver files from Part 2:

**`components/mcp9808/CMakeLists.txt`**:
```cmake
idf_component_register(
    SRCS "mcp9808.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver" "esp_timer"
)
```

**`components/mcp9808/include/mcp9808.h`**:
```c
#ifndef MCP9808_H
#define MCP9808_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define MCP9808_I2C_ADDR_DEFAULT    0x18
#define MCP9808_REG_CONFIG          0x01
#define MCP9808_REG_TEMP_UPPER      0x02
#define MCP9808_REG_TEMP_LOWER      0x03
#define MCP9808_REG_TEMP_CRIT       0x04
#define MCP9808_REG_TEMP_AMBIENT    0x05
#define MCP9808_REG_MANUFACTURER_ID 0x06
#define MCP9808_REG_DEVICE_ID       0x07
#define MCP9808_REG_RESOLUTION      0x08

#define MCP9808_MANUFACTURER_ID     0x0054
#define MCP9808_DEVICE_ID           0x04

esp_err_t mcp9808_init(void);
bool mcp9808_is_present(void);
esp_err_t mcp9808_read_temperature(float *temperature);
esp_err_t mcp9808_read_manufacturer_id(uint16_t *manufacturer_id);
esp_err_t mcp9808_read_device_id(uint16_t *device_id);

#endif // MCP9808_H
```

**`components/mcp9808/mcp9808.c`**:
```c
#include "mcp9808.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "mcp9808";

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_TIMEOUT_MS 1000

static esp_err_t mcp9808_write_register(uint8_t reg_addr, uint16_t data) {
    uint8_t write_buf[3];
    write_buf[0] = reg_addr;
    write_buf[1] = (data >> 8) & 0xFF;
    write_buf[2] = data & 0xFF;
    
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

static esp_err_t mcp9808_read_register(uint8_t reg_addr, uint16_t *data) {
    uint8_t read_buf[2];
    
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
    
    if (!mcp9808_is_present()) {
        ESP_LOGE(TAG, "MCP9808 not found on I2C bus at address 0x%02X", 
                 MCP9808_I2C_ADDR_DEFAULT);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "MCP9808 detected on I2C bus");
    
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
    
    raw_temp = raw_temp & 0x1FFF;
    
    if (raw_temp & 0x1000) {
        raw_temp = raw_temp & 0x0FFF;
        *temperature = -(float)raw_temp * 0.0625f;
    } else {
        *temperature = (float)raw_temp * 0.0625f;
    }
    
    ESP_LOGD(TAG, "Temperature: %.4f°C (raw: 0x%04X)", *temperature, raw_temp);
    return ESP_OK;
}
```

---

## Step 4: Create Main Application

Edit `main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        "driver"
        "esp_wifi"
        "esp_http_server"
        "nvs_flash"
        "esp_netif"
        "mcp9808"
        "esp_timer"
)
```

Edit `main/main.c`:

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "mcp9808.h"

static const char *TAG = "main";

// WiFi AP configuration
#define WIFI_SSID      "ESP32-Temperature"
#define WIFI_PASS      "temperature"
#define WIFI_CHANNEL   1
#define MAX_STA_CONN   4

// I2C configuration
#define I2C_MASTER_SCL_IO    48
#define I2C_MASTER_SDA_IO    47
#define I2C_MASTER_FREQ_HZ   100000

// Global state
typedef struct {
    float temperature;
    uint64_t last_update_ms;
    bool sensor_available;
    uint32_t read_count;
    uint32_t error_count;
} sensor_state_t;

static sensor_state_t g_sensor_state = {
    .temperature = 0.0,
    .last_update_ms = 0,
    .sensor_available = false,
    .read_count = 0,
    .error_count = 0
};

// HTTP server handle
static httpd_handle_t server = NULL;

/* ============================================
 * I2C Initialization
 * ============================================ */
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(err));
        return err;
    }
    
    err = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    return ESP_OK;
}

/* ============================================
 * Temperature Monitoring Task
 * ============================================ */
static void temperature_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Temperature monitoring task started");
    
    g_sensor_state.sensor_available = mcp9808_is_present();
    
    if (!g_sensor_state.sensor_available) {
        ESP_LOGW(TAG, "MCP9808 not detected, will retry periodically");
    }
    
    while (1) {
        if (!mcp9808_is_present()) {
            if (g_sensor_state.sensor_available) {
                ESP_LOGW(TAG, "MCP9808 sensor lost");
                g_sensor_state.sensor_available = false;
            }
            g_sensor_state.error_count++;
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        if (!g_sensor_state.sensor_available) {
            ESP_LOGI(TAG, "MCP9808 sensor detected");
            g_sensor_state.sensor_available = true;
        }
        
        float temperature;
        esp_err_t ret = mcp9808_read_temperature(&temperature);
        
        if (ret == ESP_OK) {
            g_sensor_state.temperature = temperature;
            g_sensor_state.last_update_ms = esp_timer_get_time() / 1000;
            g_sensor_state.read_count++;
            
            ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
        } else {
            g_sensor_state.error_count++;
            ESP_LOGE(TAG, "Failed to read temperature: %s", esp_err_to_name(ret));
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Read every 2 seconds
    }
}

/* ============================================
 * HTTP Handlers
 * ============================================ */

// GET /api/temperature - Returns current temperature as JSON
static esp_err_t api_temperature_get_handler(httpd_req_t *req) {
    char json_response[256];
    
    uint64_t current_time = esp_timer_get_time() / 1000;
    uint64_t data_age = current_time - g_sensor_state.last_update_ms;
    
    snprintf(json_response, sizeof(json_response),
        "{"
        "\"temperature\":%.2f,"
        "\"available\":%s,"
        "\"lastUpdate\":%llu,"
        "\"dataAge\":%llu,"
        "\"readCount\":%lu,"
        "\"errorCount\":%lu"
        "}",
        g_sensor_state.temperature,
        g_sensor_state.sensor_available ? "true" : "false",
        g_sensor_state.last_update_ms,
        data_age,
        g_sensor_state.read_count,
        g_sensor_state.error_count
    );
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_response);
    
    return ESP_OK;
}

// GET / - Returns basic HTML page
static esp_err_t root_get_handler(httpd_req_t *req) {
    const char* html = 
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>Temperature Monitor</title>"
        "<style>"
        "body { font-family: Arial; margin: 40px; background: #f0f0f0; }"
        ".container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
        "h1 { color: #333; }"
        ".temp { font-size: 48px; color: #007bff; margin: 20px 0; }"
        ".info { color: #666; margin: 10px 0; }"
        ".button { background: #007bff; color: white; border: none; padding: 10px 20px; margin: 5px; cursor: pointer; border-radius: 5px; }"
        ".button:hover { background: #0056b3; }"
        "#status { padding: 10px; margin: 10px 0; border-radius: 5px; }"
        ".online { background: #d4edda; color: #155724; }"
        ".offline { background: #f8d7da; color: #721c24; }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='container'>"
        "<h1>ESP32 Temperature Monitor</h1>"
        "<div id='status' class='offline'>Connecting...</div>"
        "<div class='temp' id='temperature'>--°C</div>"
        "<div class='info'>Last Update: <span id='lastUpdate'>--</span></div>"
        "<div class='info'>Read Count: <span id='readCount'>--</span></div>"
        "<div class='info'>Error Count: <span id='errorCount'>--</span></div>"
        "<div style='margin-top: 20px;'>"
        "<button class='button'>Enable</button>"
        "<button class='button'>Disable</button>"
        "<button class='button'>Reset Stats</button>"
        "</div>"
        "</div>"
        "<script>"
        "function updateData() {"
        "  fetch('/api/temperature')"
        "    .then(r => r.json())"
        "    .then(data => {"
        "      document.getElementById('temperature').textContent = data.temperature.toFixed(2) + '°C';"
        "      document.getElementById('lastUpdate').textContent = new Date(data.lastUpdate).toLocaleTimeString();"
        "      document.getElementById('readCount').textContent = data.readCount;"
        "      document.getElementById('errorCount').textContent = data.errorCount;"
        "      const status = document.getElementById('status');"
        "      if(data.available) {"
        "        status.textContent = 'Sensor Online';"
        "        status.className = 'online';"
        "      } else {"
        "        status.textContent = 'Sensor Offline';"
        "        status.className = 'offline';"
        "      }"
        "    })"
        "    .catch(e => {"
        "      document.getElementById('status').textContent = 'Connection Error';"
        "      document.getElementById('status').className = 'offline';"
        "    });"
        "}"
        "updateData();"
        "setInterval(updateData, 2000);"
        "</script>"
        "</body>"
        "</html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

/* ============================================
 * HTTP Server
 * ============================================ */
static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        httpd_uri_t api_temp_uri = {
            .uri       = "/api/temperature",
            .method    = HTTP_GET,
            .handler   = api_temperature_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &api_temp_uri);
        
        ESP_LOGI(TAG, "HTTP server started successfully");
        return server;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}

/* ============================================
 * WiFi AP Setup
 * ============================================ */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP started. SSID:%s password:%s channel:%d",
             WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
    
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_get_ip_info(netif, &ip_info);
    
    ESP_LOGI(TAG, "Access Point IP: " IPSTR, IP2STR(&ip_info.ip));
}

/* ============================================
 * Main Application
 * ============================================ */
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32 Temperature Monitor Starting ===");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize I2C
    ESP_LOGI(TAG, "Initializing I2C...");
    ESP_ERROR_CHECK(i2c_master_init());
    
    // Initialize MCP9808
    ESP_LOGI(TAG, "Initializing MCP9808...");
    ret = mcp9808_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MCP9808 initialized successfully");
    } else {
        ESP_LOGW(TAG, "MCP9808 initialization failed, monitoring will continue with retries");
    }
    
    // Start temperature monitoring task
    xTaskCreate(temperature_monitor_task, "temp_monitor", 4096, NULL, 5, NULL);
    
    // Initialize WiFi AP
    ESP_LOGI(TAG, "Starting WiFi Access Point...");
    wifi_init_softap();
    
    // Start web server
    ESP_LOGI(TAG, "Starting web server...");
    start_webserver();
    
    ESP_LOGI(TAG, "=== System Ready ===");
    ESP_LOGI(TAG, "Connect to WiFi: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Password: %s", WIFI_PASS);
    ESP_LOGI(TAG, "Then browse to: http://192.168.4.1");
}
```

---

## Step 5: Configure Project

Edit `sdkconfig.defaults`:

```
# ESP32-S3 Configuration
CONFIG_IDF_TARGET="esp32s3"

# Flash size (16MB)
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="16MB"

# PSRAM (8MB)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# Partition table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"

# HTTP Server
CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024
CONFIG_HTTPD_MAX_URI_LEN=512

# WiFi
CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

Create `partitions.csv`:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
www,      data, spiffs,  ,        512K,
```

---

## Step 6: Build and Flash

```bash
cd ~/Work/temperature-monitor

# Set up ESP-IDF environment
cd ~/esp/esp-idf
. ./export.sh
cd ~/Work/temperature-monitor

# Configure for ESP32-S3
idf.py set-target esp32s3

# Build
idf.py build

# Find your port
ls /dev/cu.usb*

# Flash (replace with your port)
idf.py -p /dev/cu.usbmodem141201 flash

# Monitor serial output
idf.py -p /dev/cu.usbmodem141201 monitor
```

---

## Step 7: Test the System

### Expected Serial Output

```
I (320) main: === ESP32 Temperature Monitor Starting ===
I (325) main: Initializing I2C...
I (330) main: I2C initialized (SDA=47, SCL=48)
I (335) main: Initializing MCP9808...
I (340) mcp9808: Initializing MCP9808 temperature sensor
I (345) mcp9808: MCP9808 detected on I2C bus
I (350) mcp9808: Manufacturer ID verified: 0x0054
I (355) mcp9808: Device ID verified: 0x04, Revision: 0x00
I (360) mcp9808: MCP9808 initialized successfully
I (365) main: MCP9808 initialized successfully
I (370) main: Starting WiFi Access Point...
I (380) main: WiFi AP started. SSID:ESP32-Temperature password:temperature channel:1
I (385) main: Access Point IP: 192.168.4.1
I (390) main: Starting web server...
I (395) main: Starting HTTP server on port 80
I (400) main: HTTP server started successfully
I (405) main: === System Ready ===
I (410) main: Connect to WiFi: ESP32-Temperature
I (415) main: Password: temperature
I (420) main: Then browse to: http://192.168.4.1
I (425) main: Temperature monitoring task started
I (2430) main: Temperature: 24.56°C
I (4435) main: Temperature: 24.50°C
I (6440) main: Temperature: 24.56°C
```

### Connect and Test

1. **Connect to WiFi**:
   - SSID: `ESP32-Temperature`
   - Password: `temperature`

2. **Open browser**: Navigate to `http://192.168.4.1`

3. **You should see**:
   - Temperature display
   - Auto-updating every 2 seconds
   - Sensor status (online/offline)
   - Read/error counts
   - Three placeholder buttons

4. **Test API**: `http://192.168.4.1/api/temperature`

**Expected JSON**:
```json
{
  "temperature": 24.56,
  "available": true,
  "lastUpdate": 1234567890,
  "dataAge": 1523,
  "readCount": 42,
  "errorCount": 0
}
```

---

## Architecture Summary

```
┌─────────────────────────────────────────────────┐
│              Application Layers                  │
├─────────────────────────────────────────────────┤
│                                                  │
│  [FreeRTOS Task: Temperature Monitor]           │
│       │                                          │
│       │ Reads every 2s                           │
│       │                                          │
│       ▼                                          │
│  [Global State: g_sensor_state]                 │
│       │                                          │
│       │ Accessed by                              │
│       │                                          │
│       ▼                                          │
│  [HTTP Server: /api/temperature]                │
│       │                                          │
│       │ Serves                                   │
│       │                                          │
│       ▼                                          │
│  [Client Browser: Auto-refresh UI]              │
│                                                  │
└─────────────────────────────────────────────────┘
```

---

## Troubleshooting

### Build Errors

**"IDF_TARGET not set"**:
```bash
idf.py set-target esp32s3
```

**"Component not found"**:
Check `main/CMakeLists.txt` has all REQUIRES

### Flash Errors

**"No serial port found"**:
```bash
ls /dev/cu.usb*
# Use the correct port in -p flag
```

**"Failed to connect"**:
- Hold BOOT button while connecting
- Check USB cable (data not charge-only)

### WiFi Issues

**"Can't find ESP32-Temperature"**:
- Check ESP32 serial output for errors
- Verify WiFi credentials in code
- Try rebooting ESP32

**"Can't connect to 192.168.4.1"**:
- Ensure you're connected to ESP32-Temperature WiFi
- Check IP in serial output
- Try 192.168.4.1 explicitly (not HTTPS)

### Sensor Issues

**"MCP9808 not found"**:
- Check wiring (SDA=GPIO47, SCL=GPIO48)
- Verify sensor has power
- Check I2C address (0x18)

---

## What We Built

✅ **Standalone ESP32 project**
✅ **I2C driver** for MCP9808
✅ **WiFi Access Point**
✅ **HTTP web server**
✅ **REST API** (`/api/temperature`)
✅ **Live-updating web page**
✅ **FreeRTOS task** for monitoring
✅ **Placeholder buttons** for future controls

---

## Next: Part 4

In Part 4, we'll replace the basic HTML with a **full Angular application** featuring:
- Professional UI with Angular Material
- Real-time temperature gauge
- Historical temperature chart
- Functional control buttons
- Responsive design

**Ready for Part 4?** Let me know when you've successfully:
1. Built and flashed the project
2. Connected to the WiFi AP
3. Accessed the web interface
4. Seen live temperature updates

Then we'll build the Angular frontend!
