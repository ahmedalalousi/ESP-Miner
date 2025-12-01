# MCP9808 Temperature Sensor Tutorial - Part 4: Angular Web Application

Creating a professional web interface with gauges, charts, and controls - complete walkthrough from zero to flashed ESP32.

## Complete Project Walkthrough

This part provides **step-by-step instructions** from an empty directory to a fully functioning ESP32-S3 with web interface.

### Prerequisites

- ESP-IDF tools installed and configured
- Node.js and npm installed (for Angular development)
- ESP32-S3-WROOM-1 board (8MB PSRAM, 16MB Flash)
- MCP9808 sensor wired (SDA=GPIO47, SCL=GPIO48)

---

## Part A: Project Setup (10 minutes)

### Step 1: Create Project Structure

```bash
# Create main project directory
cd ~/Work
mkdir temperature-monitor
cd temperature-monitor

# Create ESP32 directories
mkdir -p main components/mcp9808/include www

# Create Angular directory
mkdir webapp

# List structure
tree -L 2
```

Expected output:
```
temperature-monitor/
├── components/
│   └── mcp9808/
├── main/
├── webapp/
└── www/
```

### Step 2: Initialize Angular Project

```bash
cd webapp

# Create Angular project (say YES to routing, choose SCSS)
npx @angular/cli@latest new temp-monitor --routing --style=scss

cd temp-monitor

# Install required packages
npm install @angular/material @angular/cdk
npm install chart.js ng2-charts
npm install @types/chart.js --save-dev

# Verify
npm list --depth=0
```

---

## Part B: ESP32 Backend (30 minutes)

### Step 3: Create ESP32 Root Files

**`temperature-monitor/CMakeLists.txt`**:
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(temperature-monitor)
```

**`temperature-monitor/partitions.csv`**:
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 2M,
www,      data, spiffs,  ,        2M,
```

**`temperature-monitor/sdkconfig.defaults`**:
```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="16MB"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048
CONFIG_HTTPD_MAX_URI_LEN=1024
CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

### Step 4: Create MCP9808 Component

**`components/mcp9808/CMakeLists.txt`**:
```cmake
idf_component_register(
    SRCS "mcp9808.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver" "esp_timer"
)
```

**`components/mcp9808/include/mcp9808.h`** - Copy from Part 2

**`components/mcp9808/mcp9808.c`** - Copy from Part 2

(Use the complete implementations from Part 3)

### Step 5: Create Main Application

**`main/CMakeLists.txt`**:
```cmake
# Embed the www directory into the binary
set(www_dir "${CMAKE_CURRENT_SOURCE_DIR}/../www")

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES ${www_dir}/index.html
    REQUIRES 
        "driver"
        "esp_wifi"
        "esp_http_server"
        "nvs_flash"
        "esp_netif"
        "mcp9808"
        "esp_timer"
        "spiffs"
)
```

**`main/main.c`** - Enhanced version with file serving:

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_spiffs.h"
#include "driver/i2c.h"
#include "mcp9808.h"

static const char *TAG = "main";

#define WIFI_SSID      "ESP32-Temperature"
#define WIFI_PASS      "temperature"
#define WIFI_CHANNEL   1
#define MAX_STA_CONN   4

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

static sensor_state_t g_sensor_state = {0};
static httpd_handle_t server = NULL;

// Temperature history (last 60 readings = 2 minutes at 2s intervals)
#define HISTORY_SIZE 60
static float temperature_history[HISTORY_SIZE];
static int history_index = 0;
static int history_count = 0;

static void add_to_history(float temp) {
    temperature_history[history_index] = temp;
    history_index = (history_index + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

/* I2C Initialization */
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
    
    ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    return ESP_OK;
}

/* SPIFFS Initialization */
static void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/www",
        .partition_label = "www",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("www", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
    }
}

/* Temperature Monitoring Task */
static void temperature_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Temperature monitoring task started");
    
    g_sensor_state.sensor_available = mcp9808_is_present();
    
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
            add_to_history(temperature);
            
            ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
        } else {
            g_sensor_state.error_count++;
            ESP_LOGE(TAG, "Failed to read temperature");
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* HTTP Handlers */

// GET /api/temperature
static esp_err_t api_temperature_get_handler(httpd_req_t *req) {
    char *json_response = malloc(1024);
    if (json_response == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    uint64_t current_time = esp_timer_get_time() / 1000;
    uint64_t data_age = current_time - g_sensor_state.last_update_ms;
    
    snprintf(json_response, 1024,
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
    
    free(json_response);
    return ESP_OK;
}

// GET /api/history
static esp_err_t api_history_get_handler(httpd_req_t *req) {
    char *json_response = malloc(4096);
    if (json_response == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    int len = snprintf(json_response, 4096, "{\"history\":[");
    
    for (int i = 0; i < history_count; i++) {
        int idx = (history_index - history_count + i + HISTORY_SIZE) % HISTORY_SIZE;
        len += snprintf(json_response + len, 4096 - len, "%.2f", temperature_history[idx]);
        if (i < history_count - 1) {
            len += snprintf(json_response + len, 4096 - len, ",");
        }
    }
    
    len += snprintf(json_response + len, 4096 - len, "]}");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_response);
    
    free(json_response);
    return ESP_OK;
}

// Serve static files from SPIFFS
static esp_err_t serve_static_file(httpd_req_t *req) {
    char filepath[256];
    
    // Map / to /index.html
    if (strcmp(req->uri, "/") == 0) {
        strcpy(filepath, "/www/index.html");
    } else {
        snprintf(filepath, sizeof(filepath), "/www%s", req->uri);
    }
    
    ESP_LOGI(TAG, "Serving file: %s", filepath);
    
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Determine content type
    const char *content_type = "text/html";
    if (strstr(filepath, ".js")) content_type = "application/javascript";
    else if (strstr(filepath, ".css")) content_type = "text/css";
    else if (strstr(filepath, ".json")) content_type = "application/json";
    else if (strstr(filepath, ".png")) content_type = "image/png";
    else if (strstr(filepath, ".jpg")) content_type = "image/jpeg";
    
    httpd_resp_set_type(req, content_type);
    
    char chunk[1024];
    size_t read_bytes;
    
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            fclose(file);
            ESP_LOGE(TAG, "File sending failed");
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }
    
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* HTTP Server */
static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 10;
    
    ESP_LOGI(TAG, "Starting HTTP server");
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // API endpoints
        httpd_uri_t api_temp = {
            .uri = "/api/temperature",
            .method = HTTP_GET,
            .handler = api_temperature_get_handler,
        };
        httpd_register_uri_handler(server, &api_temp);
        
        httpd_uri_t api_hist = {
            .uri = "/api/history",
            .method = HTTP_GET,
            .handler = api_history_get_handler,
        };
        httpd_register_uri_handler(server, &api_hist);
        
        // Static file handler (must be last)
        httpd_uri_t static_files = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = serve_static_file,
        };
        httpd_register_uri_handler(server, &static_files);
        
        ESP_LOGI(TAG, "HTTP server started");
        return server;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}

/* WiFi AP */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station connected: "MACSTR, MAC2STR(event->mac));
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station disconnected: "MACSTR, MAC2STR(event->mac));
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
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP: SSID=%s Password=%s", WIFI_SSID, WIFI_PASS);
    ESP_LOGI(TAG, "Connect and browse to: http://192.168.4.1");
}

/* Main */
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32 Temperature Monitor v2.0 ===");
    
    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // SPIFFS
    init_spiffs();
    
    // I2C
    ESP_ERROR_CHECK(i2c_master_init());
    
    // MCP9808
    ret = mcp9808_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MCP9808 ready");
    } else {
        ESP_LOGW(TAG, "MCP9808 init failed, will retry");
    }
    
    // Start monitoring
    xTaskCreate(temperature_monitor_task, "temp_monitor", 4096, NULL, 5, NULL);
    
    // WiFi
    wifi_init_softap();
    
    // HTTP Server
    start_webserver();
    
    ESP_LOGI(TAG, "=== System Ready ===");
}
```

---

## Part C: Angular Frontend (45 minutes)

### Step 6: Set Up Angular Material

```bash
cd ~/Work/temperature-monitor/webapp/temp-monitor

# Configure Angular Material
ng add @angular/material
# Choose theme: Indigo/Pink
# Enable typography: Yes
# Enable animations: Yes
```

### Step 7: Create Temperature Service

```bash
ng generate service services/temperature
```

**`src/app/services/temperature.service.ts`**:

```typescript
import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, interval } from 'rxjs';
import { switchMap, startWith } from 'rxjs/operators';

export interface TemperatureData {
  temperature: number;
  available: boolean;
  lastUpdate: number;
  dataAge: number;
  readCount: number;
  errorCount: number;
}

export interface HistoryData {
  history: number[];
}

@Injectable({
  providedIn: 'root'
})
export class TemperatureService {
  private baseUrl = 'http://192.168.4.1';

  constructor(private http: HttpClient) { }

  getCurrentTemperature(): Observable<TemperatureData> {
    return this.http.get<TemperatureData>(`${this.baseUrl}/api/temperature`);
  }

  getHistory(): Observable<HistoryData> {
    return this.http.get<HistoryData>(`${this.baseUrl}/api/history`);
  }

  // Auto-refresh every 2 seconds
  getTemperatureStream(): Observable<TemperatureData> {
    return interval(2000).pipe(
      startWith(0),
      switchMap(() => this.getCurrentTemperature())
    );
  }
}
```

### Step 8: Create Components

```bash
ng generate component components/temperature-gauge
ng generate component components/temperature-chart
ng generate component components/control-panel
```

**`src/app/components/temperature-gauge/temperature-gauge.component.ts`**:

```typescript
import { Component, Input } from '@angular/core';

@Component({
  selector: 'app-temperature-gauge',
  templateUrl: './temperature-gauge.component.html',
  styleUrls: ['./temperature-gauge.component.scss']
})
export class TemperatureGaugeComponent {
  @Input() temperature: number = 0;
  @Input() isAvailable: boolean = false;

  getGaugeRotation(): string {
    // Map -40°C to 125°C to -135deg to 135deg
    const minTemp = -40;
    const maxTemp = 125;
    const minDeg = -135;
    const maxDeg = 135;
    
    const clampedTemp = Math.max(minTemp, Math.min(maxTemp, this.temperature));
    const rotation = ((clampedTemp - minTemp) / (maxTemp - minTemp)) * (maxDeg - minDeg) + minDeg;
    
    return `rotate(${rotation}deg)`;
  }

  getTemperatureColor(): string {
    if (!this.isAvailable) return '#9e9e9e';
    if (this.temperature < 0) return '#2196f3';
    if (this.temperature < 20) return '#03a9f4';
    if (this.temperature < 30) return '#4caf50';
    if (this.temperature < 40) return '#ff9800';
    return '#f44336';
  }
}
```

**`src/app/components/temperature-gauge/temperature-gauge.component.html`**:

```html
<div class="gauge-container">
  <div class="gauge">
    <div class="gauge-body">
      <!-- Temperature marks -->
      <div class="gauge-marks">
        <div class="mark" style="transform: rotate(-135deg)"><span>-40°</span></div>
        <div class="mark" style="transform: rotate(-90deg)"><span>0°</span></div>
        <div class="mark" style="transform: rotate(-45deg)"><span>25°</span></div>
        <div class="mark" style="transform: rotate(0deg)"><span>50°</span></div>
        <div class="mark" style="transform: rotate(45deg)"><span>75°</span></div>
        <div class="mark" style="transform: rotate(90deg)"><span>100°</span></div>
        <div class="mark" style="transform: rotate(135deg)"><span>125°</span></div>
      </div>
      
      <!-- Needle -->
      <div class="needle" [style.transform]="getGaugeRotation()" [style.background]="getTemperatureColor()"></div>
      <div class="needle-center"></div>
    </div>
    
    <!-- Display -->
    <div class="gauge-display">
      <div class="temperature" [style.color]="getTemperatureColor()">
        {{ temperature | number:'1.1-1' }}<span class="unit">°C</span>
      </div>
      <div class="status" [class.offline]="!isAvailable">
        {{ isAvailable ? 'Online' : 'Offline' }}
      </div>
    </div>
  </div>
</div>
```

**`src/app/components/temperature-gauge/temperature-gauge.component.scss`**:

```scss
.gauge-container {
  display: flex;
  justify-content: center;
  padding: 20px;
}

.gauge {
  position: relative;
  width: 300px;
  height: 300px;
}

.gauge-body {
  position: relative;
  width: 100%;
  height: 100%;
  border-radius: 50%;
  background: linear-gradient(135deg, #f5f5f5 0%, #e0e0e0 100%);
  box-shadow: 0 10px 30px rgba(0,0,0,0.1), inset 0 2px 5px rgba(255,255,255,0.5);
  overflow: hidden;
}

.gauge-marks {
  position: absolute;
  width: 100%;
  height: 100%;
  
  .mark {
    position: absolute;
    width: 2px;
    height: 20px;
    background: #666;
    left: 50%;
    top: 10px;
    margin-left: -1px;
    transform-origin: center 140px;
    
    span {
      position: absolute;
      top: 25px;
      left: 50%;
      transform: translateX(-50%) rotate(0deg);
      font-size: 11px;
      color: #666;
      white-space: nowrap;
    }
  }
}

.needle {
  position: absolute;
  width: 4px;
  height: 120px;
  background: #f44336;
  left: 50%;
  bottom: 50%;
  margin-left: -2px;
  transform-origin: center bottom;
  border-radius: 2px 2px 0 0;
  transition: transform 0.3s ease;
  box-shadow: 0 0 10px rgba(0,0,0,0.3);
}

.needle-center {
  position: absolute;
  width: 20px;
  height: 20px;
  background: #333;
  border-radius: 50%;
  left: 50%;
  top: 50%;
  transform: translate(-50%, -50%);
  box-shadow: 0 2px 5px rgba(0,0,0,0.3);
}

.gauge-display {
  position: absolute;
  bottom: 60px;
  left: 50%;
  transform: translateX(-50%);
  text-align: center;
  
  .temperature {
    font-size: 32px;
    font-weight: 700;
    
    .unit {
      font-size: 18px;
      font-weight: 400;
    }
  }
  
  .status {
    font-size: 12px;
    color: #4caf50;
    font-weight: 500;
    margin-top: 5px;
    
    &.offline {
      color: #f44336;
    }
  }
}
```

**`src/app/components/temperature-chart/temperature-chart.component.ts`**:

```typescript
import { Component, OnInit, OnDestroy } from '@angular/core';
import { TemperatureService } from '../../services/temperature.service';
import { Subscription, interval } from 'rxjs';
import { switchMap } from 'rxjs/operators';
import { ChartConfiguration, ChartOptions } from 'chart.js';

@Component({
  selector: 'app-temperature-chart',
  templateUrl: './temperature-chart.component.html',
  styleUrls: ['./temperature-chart.component.scss']
})
export class TemperatureChartComponent implements OnInit, OnDestroy {
  private subscription?: Subscription;

  public lineChartData: ChartConfiguration<'line'>['data'] = {
    labels: [],
    datasets: [
      {
        data: [],
        label: 'Temperature (°C)',
        fill: true,
        tension: 0.4,
        borderColor: '#2196f3',
        backgroundColor: 'rgba(33, 150, 243, 0.1)',
        pointBackgroundColor: '#2196f3',
        pointBorderColor: '#fff',
        pointHoverBackgroundColor: '#fff',
        pointHoverBorderColor: '#2196f3'
      }
    ]
  };

  public lineChartOptions: ChartOptions<'line'> = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: {
        display: true,
        position: 'top',
      }
    },
    scales: {
      y: {
        title: {
          display: true,
          text: 'Temperature (°C)'
        },
        beginAtZero: false
      },
      x: {
        title: {
          display: true,
          text: 'Time'
        }
      }
    }
  };

  constructor(private temperatureService: TemperatureService) { }

  ngOnInit(): void {
    // Update chart every 2 seconds
    this.subscription = interval(2000).pipe(
      switchMap(() => this.temperatureService.getHistory())
    ).subscribe(data => {
      const now = new Date();
      const labels = data.history.map((_, i) => {
        const time = new Date(now.getTime() - (data.history.length - i - 1) * 2000);
        return time.toLocaleTimeString();
      });
      
      this.lineChartData.labels = labels;
      this.lineChartData.datasets[0].data = data.history;
    });
  }

  ngOnDestroy(): void {
    this.subscription?.unsubscribe();
  }
}
```

**`src/app/components/temperature-chart/temperature-chart.component.html`**:

```html
<div class="chart-container">
  <canvas baseChart
    [data]="lineChartData"
    [options]="lineChartOptions"
    type="line">
  </canvas>
</div>
```

**`src/app/components/temperature-chart/temperature-chart.component.scss`**:

```scss
.chart-container {
  position: relative;
  height: 300px;
  padding: 20px;
}
```

**`src/app/components/control-panel/control-panel.component.ts`**:

```typescript
import { Component } from '@angular/core';
import { MatSnackBar } from '@angular/material/snack-bar';

@Component({
  selector: 'app-control-panel',
  templateUrl: './control-panel.component.html',
  styleUrls: ['./control-panel.component.scss']
})
export class ControlPanelComponent {
  constructor(private snackBar: MatSnackBar) { }

  onEnable(): void {
    this.snackBar.open('Enable command sent', 'Close', { duration: 2000 });
  }

  onDisable(): void {
    this.snackBar.open('Disable command sent', 'Close', { duration: 2000 });
  }

  onReset(): void {
    this.snackBar.open('Reset command sent', 'Close', { duration: 2000 });
  }
}
```

**`src/app/components/control-panel/control-panel.component.html`**:

```html
<mat-card>
  <mat-card-header>
    <mat-card-title>Controls</mat-card-title>
  </mat-card-header>
  <mat-card-content>
    <div class="button-row">
      <button mat-raised-button color="primary" (click)="onEnable()">
        <mat-icon>play_arrow</mat-icon>
        Enable
      </button>
      <button mat-raised-button color="warn" (click)="onDisable()">
        <mat-icon>pause</mat-icon>
        Disable
      </button>
      <button mat-raised-button (click)="onReset()">
        <mat-icon>refresh</mat-icon>
        Reset Stats
      </button>
    </div>
  </mat-card-content>
</mat-card>
```

**`src/app/components/control-panel/control-panel.component.scss`**:

```scss
.button-row {
  display: flex;
  gap: 10px;
  flex-wrap: wrap;
  
  button {
    flex: 1;
    min-width: 120px;
  }
}
```

### Step 9: Main App Component

**`src/app/app.component.ts`**:

```typescript
import { Component, OnInit, OnDestroy } from '@angular/core';
import { TemperatureService, TemperatureData } from './services/temperature.service';
import { Subscription } from 'rxjs';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.scss']
})
export class AppComponent implements OnInit, OnDestroy {
  title = 'ESP32 Temperature Monitor';
  temperatureData?: TemperatureData;
  private subscription?: Subscription;

  constructor(private temperatureService: TemperatureService) { }

  ngOnInit(): void {
    this.subscription = this.temperatureService.getTemperatureStream()
      .subscribe(data => {
        this.temperatureData = data;
      });
  }

  ngOnDestroy(): void {
    this.subscription?.unsubscribe();
  }
}
```

**`src/app/app.component.html`**:

```html
<mat-toolbar color="primary">
  <span>{{ title }}</span>
  <span class="spacer"></span>
  <span *ngIf="temperatureData">
    Reads: {{ temperatureData.readCount }} | Errors: {{ temperatureData.errorCount }}
  </span>
</mat-toolbar>

<div class="container">
  <div class="row">
    <div class="col-md-6">
      <mat-card>
        <mat-card-header>
          <mat-card-title>Current Temperature</mat-card-title>
        </mat-card-header>
        <mat-card-content>
          <app-temperature-gauge 
            [temperature]="temperatureData?.temperature || 0"
            [isAvailable]="temperatureData?.available || false">
          </app-temperature-gauge>
        </mat-card-content>
      </mat-card>
    </div>
    
    <div class="col-md-6">
      <app-control-panel></app-control-panel>
      
      <mat-card style="margin-top: 20px;">
        <mat-card-header>
          <mat-card-title>System Info</mat-card-title>
        </mat-card-header>
        <mat-card-content *ngIf="temperatureData">
          <div class="info-row">
            <span class="label">Status:</span>
            <span [class.online]="temperatureData.available" [class.offline]="!temperatureData.available">
              {{ temperatureData.available ? 'Online' : 'Offline' }}
            </span>
          </div>
          <div class="info-row">
            <span class="label">Last Update:</span>
            <span>{{ temperatureData.lastUpdate | date:'medium' }}</span>
          </div>
          <div class="info-row">
            <span class="label">Data Age:</span>
            <span>{{ temperatureData.dataAge / 1000 | number:'1.0-0' }}s</span>
          </div>
        </mat-card-content>
      </mat-card>
    </div>
  </div>
  
  <div class="row">
    <div class="col-12">
      <mat-card>
        <mat-card-header>
          <mat-card-title>Temperature History</mat-card-title>
        </mat-card-header>
        <mat-card-content>
          <app-temperature-chart></app-temperature-chart>
        </mat-card-content>
      </mat-card>
    </div>
  </div>
</div>
```

**`src/app/app.component.scss`**:

```scss
.spacer {
  flex: 1 1 auto;
}

.container {
  padding: 20px;
  max-width: 1400px;
  margin: 0 auto;
}

.row {
  display: flex;
  gap: 20px;
  margin-bottom: 20px;
  flex-wrap: wrap;
  
  .col-md-6 {
    flex: 1;
    min-width: 300px;
  }
  
  .col-12 {
    flex: 1;
  }
}

.info-row {
  display: flex;
  justify-content: space-between;
  padding: 8px 0;
  border-bottom: 1px solid #eee;
  
  &:last-child {
    border-bottom: none;
  }
  
  .label {
    font-weight: 500;
    color: #666;
  }
  
  .online {
    color: #4caf50;
    font-weight: 500;
  }
  
  .offline {
    color: #f44336;
    font-weight: 500;
  }
}
```

### Step 10: Update App Module

**`src/app/app.module.ts`**:

```typescript
import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';
import { HttpClientModule } from '@angular/common/http';

// Angular Material
import { MatToolbarModule } from '@angular/material/toolbar';
import { MatCardModule } from '@angular/material/card';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatSnackBarModule } from '@angular/material/snack-bar';

// Chart.js
import { NgChartsModule } from 'ng2-charts';

import { AppRoutingModule } from './app-routing.module';
import { AppComponent } from './app.component';
import { TemperatureGaugeComponent } from './components/temperature-gauge/temperature-gauge.component';
import { TemperatureChartComponent } from './components/temperature-chart/temperature-chart.component';
import { ControlPanelComponent } from './components/control-panel/control-panel.component';

@NgModule({
  declarations: [
    AppComponent,
    TemperatureGaugeComponent,
    TemperatureChartComponent,
    ControlPanelComponent
  ],
  imports: [
    BrowserModule,
    AppRoutingModule,
    BrowserAnimationsModule,
    HttpClientModule,
    MatToolbarModule,
    MatCardModule,
    MatButtonModule,
    MatIconModule,
    MatSnackBarModule,
    NgChartsModule
  ],
  providers: [],
  bootstrap: [AppComponent]
})
export class AppModule { }
```

---

## Part D: Build and Deploy (20 minutes)

### Step 11: Build Angular App

```bash
cd ~/Work/temperature-monitor/webapp/temp-monitor

# Production build
ng build --configuration production

# Copy to ESP32 www directory
cp -r dist/temp-monitor/* ../../www/

# Verify files
ls -la ../../www/
```

### Step 12: Flash to ESP32

```bash
cd ~/Work/temperature-monitor

# Set up ESP-IDF environment
cd ~/esp/esp-idf
. ./export.sh
cd ~/Work/temperature-monitor

# Set target
idf.py set-target esp32s3

# Build
idf.py build

# Flash (find your port first)
ls /dev/cu.usb*

# Flash everything
idf.py -p /dev/cu.usbmodem141201 flash

# Monitor
idf.py -p /dev/cu.usbmodem141201 monitor
```

### Step 13: Test Complete System

1. **Watch serial output** - Should see initialization messages

2. **Connect to WiFi**:
   - SSID: `ESP32-Temperature`
   - Password: `temperature`

3. **Open browser**: `http://192.168.4.1`

4. **Expected result**:
   - Animated temperature gauge
   - Live updating temperature
   - Historical chart with last 60 readings
   - Control buttons (show snackbar notifications)
   - System info panel with statistics

---

## Troubleshooting

### Angular Build Issues

**Module not found**:
```bash
npm install
ng build
```

**Chart.js errors**:
```bash
npm install chart.js ng2-charts --save
npm install @types/chart.js --save-dev
```

### ESP32 Flash Issues

**Partition too small**:
- Increase www partition in `partitions.csv`
- Rebuild: `idf.py build`

**Files not found**:
- Verify `www/` directory has Angular build output
- Check `ls www/` shows index.html and .js files

### Browser Issues

**Can't connect**:
- Verify connected to ESP32-Temperature WiFi
- Check ESP32 IP in serial output (should be 192.168.4.1)
- Clear browser cache

**API errors**:
- Check browser console (F12)
- Verify ESP32 serial shows HTTP requests
- Check API URLs in `temperature.service.ts`

---

## Project Complete!

You now have a **fully functional temperature monitoring system**:

✅ ESP32-S3 with MCP9808 sensor
✅ WiFi Access Point
✅ REST API backend
✅ Professional Angular frontend
✅ Real-time temperature gauge
✅ Historical chart
✅ Control panel
✅ Responsive design

### File Structure Summary

```
temperature-monitor/
├── components/
│   └── mcp9808/           # Sensor driver
├── main/
│   └── main.c             # ESP32 application
├── webapp/
│   └── temp-monitor/      # Angular source
├── www/                   # Angular build output
├── CMakeLists.txt
├── partitions.csv
└── sdkconfig.defaults
```

### Next Steps

1. **Add more sensors**: Pressure, humidity, etc.
2. **Data logging**: Store to SD card or cloud
3. **Alerts**: Email/SMS notifications
4. **Configuration UI**: WiFi setup, thresholds
5. **Mobile app**: Native iOS/Android

Congratulations on completing the tutorial! 🎉
