# Voltage Monitor Component Explained

Understanding how the voltage monitor fits into ESP-Miner's architecture.

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│  Web UI (Browser)                               │
│  - Displays voltage values                      │
│  - Shows charts                                 │
└────────────┬────────────────────────────────────┘
             │ HTTP API / WebSocket
             ↓
┌─────────────────────────────────────────────────┐
│  HTTP Server (main/http_server/)                │
│  - Serves API endpoints                         │
│  - Reads from GLOBAL_STATE                      │
└────────────┬────────────────────────────────────┘
             │ Shared Memory
             ↓
┌─────────────────────────────────────────────────┐
│  GLOBAL_STATE Structure                         │
│  - voltage_monitor_data                         │
│  - Shared across all tasks                      │
└────────────┬────────────────────────────────────┘
             │ Write
             ↑ Read
┌─────────────────────────────────────────────────┐
│  Voltage Monitor Task                           │
│  - FreeRTOS task (runs continuously)            │
│  - Reads from ADC                               │
│  - Stores in GLOBAL_STATE                       │
└────────────┬────────────────────────────────────┘
             │ I2C Communication
             ↓
┌─────────────────────────────────────────────────┐
│  ADS1115 ADC (Hardware)                         │
│  - 16-bit analog-to-digital converter           │
│  - Connected via I2C                            │
│  - Measures ASIC voltages                       │
└─────────────────────────────────────────────────┘
```

## Component Structure

### Files

```
components/asic/
├── voltage_monitor.c       # Main implementation
├── voltage_monitor.h       # Not shown (header file)
└── include/
    └── voltage_monitor.h   # Public interface
```

### Key Functions

#### 1. Initialization

**`voltage_monitor_init()`**
- Called once at startup
- Detects if ADS1115 is present
- Configures ADC if found
- Creates FreeRTOS task

#### 2. Main Task Loop

**`voltage_monitor_task()`**
- Runs continuously in background
- Reads ADC values periodically
- Stores results in GLOBAL_STATE
- Never exits (infinite loop)

#### 3. Data Access

**`voltage_monitor_get_data()`**
- Called by other components
- Returns voltage data from GLOBAL_STATE
- Thread-safe access

## Data Flow

### 1. Hardware → Driver

```
ADS1115 (I2C Address 0x48)
    ↓
I2C Bus (GPIO 47 SDA, GPIO 48 SCL)
    ↓
ESP32 I2C Driver
    ↓
voltage_monitor.c reads register
```

**Code**:
```c
// Read from ADS1115
uint8_t read_buf[2];
i2c_master_read_from_device(
    I2C_NUM_0,
    ADS1115_ADDR,  // 0x48
    read_buf,
    2,
    pdMS_TO_TICKS(1000)
);

// Convert to voltage
int16_t raw = (read_buf[0] << 8) | read_buf[1];
float voltage = (raw * 6.144f) / 32768.0f;
```

### 2. Driver → Global State

```c
// Store in global state (shared memory)
GLOBAL_STATE->voltage_monitor.chain[0].voltage = voltage;
GLOBAL_STATE->voltage_monitor.chain[0].timestamp = esp_timer_get_time();
GLOBAL_STATE->voltage_monitor.enabled = true;
```

**GLOBAL_STATE** is a structure shared across all tasks:

```c
typedef struct {
    // ... other fields
    voltage_monitor_data_t voltage_monitor;
} GlobalState;

// Defined once, accessed everywhere
extern GlobalState *GLOBAL_STATE;
```

### 3. Global State → HTTP Server

```c
// In http_server.c
static esp_err_t voltage_api_handler(httpd_req_t *req) {
    // Read from global state
    float voltage = GLOBAL_STATE->voltage_monitor.chain[0].voltage;
    
    // Create JSON response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "voltage", voltage);
    
    // Send to browser
    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, json);
    
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}
```

### 4. HTTP Server → Web UI

```typescript
// In Angular service
getVoltage(): Observable<VoltageData> {
  return this.http.get<VoltageData>('/api/voltage');
}

// In component
ngOnInit() {
  this.voltageService.getVoltage().subscribe(data => {
    this.voltage = data.voltage;
  });
}
```

## Task Synchronisation

### FreeRTOS Task

The voltage monitor runs as a **separate task**:

```c
void voltage_monitor_task(void *pvParameters) {
    while(1) {
        // 1. Read from hardware
        float voltage = read_adc_voltage();
        
        // 2. Update global state
        GLOBAL_STATE->voltage_monitor.voltage = voltage;
        
        // 3. Sleep (yield to other tasks)
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second
    }
}
```

**Task is created in `voltage_monitor_init()`**:

```c
xTaskCreate(
    voltage_monitor_task,    // Function
    "voltage_monitor",       // Name
    4096,                    // Stack size
    NULL,                    // Parameters
    5,                       // Priority
    &task_handle             // Handle
);
```

### Multiple Tasks Running

```
Time →
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Task: Voltage Monitor
│   Read ADC   │ Sleep 1s │   Read ADC   │ Sleep 1s │
└─────┬────────┘          └─────┬────────┘
      │                         │
      ↓ Write                   ↓ Write
  GLOBAL_STATE             GLOBAL_STATE

Task: HTTP Server
      │ Handle  │  Handle  │   Handle   │  Handle  │
      │Request 1│ Request 2│  Request 3 │ Request 4│
      └────┬────┘          └─────┬──────┘
           │                     │
           ↓ Read                ↓ Read
       GLOBAL_STATE          GLOBAL_STATE

Task: Power Management
│   Adjust    │    Sleep    │   Adjust    │  Sleep  │
│  Voltage    │             │  Voltage    │         │
```

## Thread Safety

**Problem**: Multiple tasks accessing shared memory

**Solution**: Use mutexes (mutual exclusion locks)

```c
// Global mutex
static SemaphoreHandle_t data_mutex;

// Initialize mutex
data_mutex = xSemaphoreCreateMutex();

// Write with protection
void voltage_monitor_update(float voltage) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);  // Lock
    GLOBAL_STATE->voltage_monitor.voltage = voltage;
    xSemaphoreGive(data_mutex);                  // Unlock
}

// Read with protection
float voltage_monitor_get(void) {
    float value;
    xSemaphoreTake(data_mutex, portMAX_DELAY);  // Lock
    value = GLOBAL_STATE->voltage_monitor.voltage;
    xSemaphoreGive(data_mutex);                  // Unlock
    return value;
}
```

## Integration Points

### 1. Component Registration

**In `main/CMakeLists.txt`**:
```cmake
set(COMPONENT_REQUIRES
    "driver"
    "esp_timer"
    "asic"           # Voltage monitor is part of asic component
)
```

### 2. Initialization in main.c

```c
void app_main(void) {
    // ... other init
    
    // Initialize voltage monitor
    voltage_monitor_init();
    
    // ... continue
}
```

### 3. API Endpoint Registration

**In `http_server.c`**:
```c
void register_api_endpoints(httpd_handle_t server) {
    // ... other endpoints
    
    httpd_uri_t voltage_uri = {
        .uri = "/api/voltage",
        .method = HTTP_GET,
        .handler = voltage_api_handler
    };
    httpd_register_uri_handler(server, &voltage_uri);
}
```

### 4. Web UI Service

**In `src/app/services/voltage.service.ts`**:
```typescript
@Injectable({providedIn: 'root'})
export class VoltageService {
  constructor(private http: HttpClient) {}
  
  getVoltage(): Observable<VoltageData> {
    return this.http.get<VoltageData>('/api/voltage');
  }
}
```

### 5. Web UI Component

**In `src/app/components/voltage-monitor/`**:
```typescript
export class VoltageMonitorComponent {
  voltage: number = 0;
  
  ngOnInit() {
    // Poll every second
    interval(1000).subscribe(() => {
      this.voltageService.getVoltage().subscribe(
        data => this.voltage = data.voltage
      );
    });
  }
}
```

## Key Takeaways

1. **FreeRTOS Task**: Runs continuously, reads hardware
2. **GLOBAL_STATE**: Shared memory for inter-task communication
3. **HTTP API**: Exposes data to web UI
4. **Thread Safety**: Mutexes prevent data corruption
5. **Separation of Concerns**:
   - Hardware access in driver
   - Data storage in global state
   - API in HTTP server
   - Display in web UI

This architecture is **modular** and **scalable** - you can add new sensors following the same pattern!
