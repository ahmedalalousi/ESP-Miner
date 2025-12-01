# MCP9808 Temperature Sensor Tutorial - Part 3: Building a Standalone Web Server

**Learning Goals**: Understanding ESP32 project structure, FreeRTOS task creation, HTTP server implementation, and RESTful API design.

---

## Architecture Overview

Before writing code, let's understand what we're building:

```
┌─────────────────────────────────────────────────────┐
│            Our Temperature Monitor System            │
├─────────────────────────────────────────────────────┤
│                                                      │
│  ┌──────────────────┐                               │
│  │  FreeRTOS Task   │  Runs independently           │
│  │  (Background)    │  Polls sensor every 2s        │
│  │                  │  Updates global state         │
│  └────────┬─────────┘                               │
│           │                                          │
│           │ Writes to                                │
│           ▼                                          │
│  ┌──────────────────┐                               │
│  │  Global State    │  Shared memory                │
│  │  (temperature,   │  Thread-safe access           │
│  │   statistics)    │                               │
│  └────────┬─────────┘                               │
│           │                                          │
│           │ Reads from                               │
│           ▼                                          │
│  ┌──────────────────┐                               │
│  │  HTTP Server     │  Handles web requests         │
│  │  - GET /         │  Serves HTML page             │
│  │  - GET /api/temp │  Returns JSON data            │
│  └──────────────────┘                               │
│                                                      │
└─────────────────────────────────────────────────────┘
```

**Key Concepts**:
1. **Separation of Concerns**: Reading sensor ≠ serving web pages
2. **Shared State**: One place to store temperature data
3. **RESTful API**: Standard HTTP methods for data access
4. **Non-blocking**: Web server doesn't wait for sensor reads

---

## Part A: Project Structure Fundamentals

### Why This Structure?

ESP-IDF uses CMake and follows a specific project layout. Understanding why helps you modify it later.

```
temperature-monitor/           # Project root
├── CMakeLists.txt            # Top-level build config
├── main/                     # Main application
│   ├── CMakeLists.txt        # Tells CMake what to compile
│   └── main.c                # Entry point (app_main)
├── components/               # Reusable components
│   └── mcp9808/             # Our sensor driver
│       ├── CMakeLists.txt
│       ├── mcp9808.c
│       └── include/
│           └── mcp9808.h
└── sdkconfig.defaults        # Default configuration
```

**Why `components/`?**
- Reusable across projects
- Clean separation of concerns
- Easy to unit test
- Can be published as libraries

**Why `main/`?**
- ESP-IDF convention (it looks for this directory)
- Contains application-specific code
- Links against components

### Create the Structure

```bash
cd ~/Work
mkdir temperature-monitor
cd temperature-monitor

# Create directories
mkdir -p main components/mcp9808/include

# Create placeholder files (we'll fill these in)
touch CMakeLists.txt
touch sdkconfig.defaults
touch main/CMakeLists.txt
touch main/main.c
touch components/mcp9808/CMakeLists.txt
```

---

## Part B: Understanding CMake Build System

### Root CMakeLists.txt

**Purpose**: Tell CMake this is an ESP-IDF project.

```cmake
# Minimum CMake version required
cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF's project system
# This finds ESP-IDF installation and sets up build environment
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define our project name
# This becomes the .bin filename
project(temperature-monitor)
```

**Line-by-line**:
1. `cmake_minimum_required`: Ensures user has compatible CMake
2. `include(...)`: Loads ESP-IDF's build system (finds components, sets compiler flags)
3. `project(...)`: Creates build target, names output file

**What happens when you run `idf.py build`?**
1. CMake reads this file
2. Scans `components/` for components
3. Reads each component's CMakeLists.txt
4. Compiles everything
5. Links into final binary
6. Creates `build/temperature-monitor.bin`

### Component CMakeLists.txt

**Purpose**: Define what gets compiled in this component.

Create `components/mcp9808/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "mcp9808.c"           # Source files to compile
    INCLUDE_DIRS "include"      # Public headers other components can use
    REQUIRES "driver" "esp_timer"  # Dependencies this component needs
)
```

**What `idf_component_register` does**:
- Registers component with build system
- `SRCS`: Files to compile (can list multiple: "file1.c" "file2.c")
- `INCLUDE_DIRS`: Headers that other components can `#include`
- `REQUIRES`: Other components we depend on
  - `driver`: Provides I2C functions
  - `esp_timer`: Provides timestamp functions

**Why separate REQUIRES?**
- Build system knows what order to compile
- Prevents circular dependencies
- Only links what you need (smaller binary)

### Main CMakeLists.txt

Create `main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        "driver"           # I2C driver
        "esp_wifi"         # WiFi support
        "esp_http_server"  # HTTP server
        "nvs_flash"        # Non-volatile storage
        "esp_netif"        # Network interface
        "mcp9808"          # Our sensor component
        "esp_timer"        # Timestamps
)
```

**Notice**:
- We list `mcp9808` in REQUIRES
- Build system finds it in `components/`
- Automatic linking happens

---

## Part C: Configuration with sdkconfig

### Understanding sdkconfig.defaults

ESP-IDF has **thousands** of configuration options. `sdkconfig.defaults` pre-configures them.

Create `sdkconfig.defaults`:

```ini
# Target chip
CONFIG_IDF_TARGET="esp32s3"

# Flash size - your board has 16MB
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="16MB"

# PSRAM - your board has 8MB
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y        # Octal mode (faster)
CONFIG_SPIRAM_SPEED_80M=y       # 80MHz speed

# Logging level
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

**Line-by-line**:

1. `CONFIG_IDF_TARGET="esp32s3"`: 
   - Tells compiler which chip you have
   - Changes available peripherals
   - Affects memory layout

2. `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`:
   - Your board has 16MB flash
   - Affects partition table layout
   - More space for web files

3. `CONFIG_SPIRAM=y`:
   - Enables external RAM (PSRAM)
   - Your board has 8MB
   - Gives more heap memory

4. `CONFIG_SPIRAM_MODE_OCT=y`:
   - Uses 8 data lines (faster)
   - ESP32-S3 specific feature

**Why configure this?**
- Optimize for your specific hardware
- Enable/disable features
- Control memory usage
- Set performance parameters

**See all options**:
```bash
idf.py menuconfig
```

---

## Part D: FreeRTOS Task Fundamentals

### What is a Task?

Think of a task as an independent thread that runs concurrently with other tasks.

**Analogy**: Your kitchen
- **Task 1**: Boiling water (runs continuously)
- **Task 2**: Chopping vegetables (runs continuously)
- **Task 3**: Answering doorbell (waits for event)

FreeRTOS scheduler decides which task runs when.

### Task Creation Pattern

```c
// 1. Define the task function
static void temperature_monitor_task(void *pvParameters) {
    // Task code here - runs forever
    while(1) {
        // Do work
        vTaskDelay(pdMS_TO_TICKS(2000));  // Sleep 2 seconds
    }
}

// 2. Create the task in app_main()
void app_main(void) {
    xTaskCreate(
        temperature_monitor_task,    // Function to run
        "temp_monitor",              // Name (for debugging)
        4096,                        // Stack size in bytes
        NULL,                        // Parameters to pass
        5,                           // Priority (0-25, higher = more important)
        NULL                         // Task handle (to control it later)
    );
}
```

**Line-by-line**:

1. **Task function signature**: `void task_name(void *pvParameters)`
   - Must return void
   - Takes generic pointer (for passing data)

2. **Infinite loop**: `while(1)`
   - Tasks never exit
   - If they do, system may crash
   - Like `main()` in embedded systems

3. **vTaskDelay()**: 
   - Yields CPU to other tasks
   - `pdMS_TO_TICKS(2000)`: Convert 2000ms to system ticks
   - Without this, task would hog CPU

4. **xTaskCreate()**:
   - Allocates stack memory (4096 bytes here)
   - Registers with scheduler
   - Task starts immediately

### Stack Size

**How to determine stack size?**

```c
// Rule of thumb:
// - Simple task: 2048 bytes
// - Network tasks: 4096 bytes
// - Heavy processing: 8192+ bytes

// Our temperature task:
4096 bytes =
    Local variables (temperature, etc.)
  + Function call overhead
  + I2C transaction buffers
  + Logging strings
  + Safety margin
```

**Too small**: Stack overflow → crash
**Too large**: Wastes RAM

**Find actual usage**:
```c
UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Stack remaining: %d bytes", stack_high_water_mark * 4);
```

### Task Priority

```
Priority 25 (highest)
    ↓
Priority 10: WiFi tasks (built-in)
    ↓
Priority 5:  Our temperature task  ← Good default
    ↓
Priority 1:  Low priority work
    ↓
Priority 0:  Idle task (always runs when nothing else does)
```

**Rules**:
- Higher number = higher priority
- Equal priority = round-robin scheduling
- Higher priority always preempts lower
- Don't set too high (starves other tasks)

---

## Part E: Shared State Management

### The Problem

```c
// ❌ WRONG: Race condition
float g_temperature;  // Global variable

// Task 1: Write
void temp_task() {
    g_temperature = 25.5;  // Write
}

// Task 2: Read
void http_handler() {
    printf("%.2f", g_temperature);  // Read
}

// What if Task 2 reads while Task 1 is writing?
// Result: Corrupted data!
```

### Solution 1: Atomic Operations

For simple types on ESP32, single reads/writes are atomic:

```c
// ✅ SAFE: 32-bit read/write is atomic on ESP32
uint32_t g_counter;

// These are safe:
g_counter = 42;           // Single write
uint32_t val = g_counter; // Single read

// ❌ NOT SAFE: Read-modify-write
g_counter++;  // Actually: read, add, write (3 operations)
```

### Solution 2: Critical Sections

For complex operations:

```c
#include "freertos/FreeRTOS.h"

// Short critical section
portENTER_CRITICAL();
g_temperature = read_sensor();
g_timestamp = get_time();
portEXIT_CRITICAL();

// Alternative: Disable interrupts briefly
taskDISABLE_INTERRUPTS();
// ... quick operation ...
taskENABLE_INTERRUPTS();
```

**When to use**:
- Very short operations (<100 μs)
- Updating multiple related variables
- Simple read-modify-write

**Don't use for**:
- I2C transactions (too slow)
- Network operations
- Long calculations

### Solution 3: Mutex (Mutual Exclusion)

For longer operations:

```c
static SemaphoreHandle_t g_state_mutex;

// Initialization
g_state_mutex = xSemaphoreCreateMutex();

// Task 1: Update
void temp_task() {
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);  // Lock
    g_state.temperature = read_sensor();
    g_state.timestamp = get_time();
    xSemaphoreGive(g_state_mutex);  // Unlock
}

// Task 2: Read
void http_handler() {
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);  // Lock
    float temp = g_state.temperature;
    xSemaphoreGive(g_state_mutex);  // Unlock
    
    // Use temp here
}
```

**Our approach**: Since we only update a few floats/ints, we'll use atomic operations (simpler).

### Designing the State Structure

```c
typedef struct {
    float temperature;          // Current reading
    uint64_t last_update_ms;   // When we got it
    bool sensor_available;      // Hardware status
    uint32_t read_count;       // Total successful reads
    uint32_t error_count;      // Total failures
} sensor_state_t;

// Global instance
static sensor_state_t g_sensor_state = {
    .temperature = 0.0,
    .last_update_ms = 0,
    .sensor_available = false,
    .read_count = 0,
    .error_count = 0
};
```

**Why this structure?**
- `temperature`: The data we care about
- `last_update_ms`: Detect stale data (sensor died?)
- `sensor_available`: Did initialization succeed?
- `read_count/error_count`: Health monitoring

**Thread safety**: Each field is a simple type, atomic on ESP32.

---

## Part F: HTTP Server Architecture

### Understanding ESP-IDF HTTP Server

ESP-IDF provides `esp_http_server` component:

```c
#include "esp_http_server.h"

// 1. Define handler function
esp_err_t my_handler(httpd_req_t *req) {
    // Process request
    httpd_resp_sendstr(req, "Hello!");
    return ESP_OK;
}

// 2. Create URI handler
httpd_uri_t my_uri = {
    .uri       = "/hello",        // URL path
    .method    = HTTP_GET,        // HTTP method
    .handler   = my_handler,      // Function to call
    .user_ctx  = NULL             // Optional context
};

// 3. Start server and register
httpd_handle_t server = NULL;
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
httpd_start(&server, &config);
httpd_register_uri_handler(server, &my_uri);
```

**How it works**:
1. Server listens on port 80
2. Client sends: `GET /hello HTTP/1.1`
3. Server matches URI `/hello`
4. Calls `my_handler()`
5. Handler sends response
6. Server closes connection

### RESTful API Design

**REST principles**:
- Use HTTP methods semantically
- Resources identified by URLs
- Stateless (each request independent)
- Return structured data (JSON)

**Our API design**:

```
GET /api/temperature
└── Returns: {
    "temperature": 24.5,
    "available": true,
    "lastUpdate": 1234567890,
    "dataAge": 2000,
    "readCount": 42,
    "errorCount": 0
}

GET /
└── Returns: HTML page
```

### Handler Pattern

```c
static esp_err_t api_temperature_handler(httpd_req_t *req) {
    // 1. Get data from global state
    float temp = g_sensor_state.temperature;
    bool available = g_sensor_state.sensor_available;
    
    // 2. Format as JSON
    char json[256];
    snprintf(json, sizeof(json),
        "{\"temperature\":%.2f,\"available\":%s}",
        temp,
        available ? "true" : "false"
    );
    
    // 3. Set response headers
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    // 4. Send response
    httpd_resp_sendstr(req, json);
    
    return ESP_OK;
}
```

**Line-by-line**:

1. **Read global state**: Quick atomic read (no locking needed)

2. **Format JSON**:
   - `snprintf()`: Safe string formatting
   - Manual JSON (simple data)
   - Alternative: Use cJSON library for complex objects

3. **Set headers**:
   - `Content-Type: application/json`: Tells browser it's JSON
   - `Access-Control-Allow-Origin: *`: Allow cross-origin requests (for development)

4. **Send response**:
   - `httpd_resp_sendstr()`: Send string and close connection
   - Alternative: `httpd_resp_send_chunk()` for streaming

### JSON Formatting Tips

**Manual JSON** (what we use):
```c
// Simple, fast, no dependencies
snprintf(json, size, "{\"key\":%d}", value);
```

**cJSON library** (for complex data):
```c
#include "cJSON.h"

cJSON *root = cJSON_CreateObject();
cJSON_AddNumberToObject(root, "temperature", temp);
cJSON_AddBoolToObject(root, "available", available);
char *json_str = cJSON_Print(root);
httpd_resp_sendstr(req, json_str);
free(json_str);
cJSON_Delete(root);
```

**When to use which?**
- Manual: < 5 fields, simple types
- cJSON: Complex nesting, arrays, many fields

---

## Part G: WiFi Access Point Setup

### WiFi Modes

ESP32 supports three WiFi modes:

1. **Station (STA)**: Connect to existing WiFi
   - Like your phone connecting to home WiFi
   - ESP32 is client

2. **Access Point (AP)**: Create WiFi network
   - Like your router
   - ESP32 is server
   - **We use this**

3. **AP+STA**: Both simultaneously
   - Connect to WiFi AND create network
   - Bridge mode

### Why Access Point Mode?

For our standalone monitor:
- No existing WiFi needed
- Works anywhere
- Simple configuration
- Direct connection to device

**Tradeoff**: No internet access (but we don't need it)

### WiFi Initialization Sequence

```c
// 1. Initialize network stack
ESP_ERROR_CHECK(esp_netif_init());

// 2. Create default event loop
ESP_ERROR_CHECK(esp_event_loop_create_default());

// 3. Create network interface for AP
esp_netif_create_default_wifi_ap();

// 4. Initialize WiFi driver
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&cfg));

// 5. Register event handler
esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &handler, NULL);

// 6. Configure AP settings
wifi_config_t wifi_config = {
    .ap = {
        .ssid = "ESP32-Temperature",
        .password = "temperature",
        .max_connection = 4,
        .authmode = WIFI_AUTH_WPA2_PSK
    }
};

// 7. Set mode and config
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

// 8. Start WiFi
ESP_ERROR_CHECK(esp_wifi_start());
```

**Line-by-line**:

1. **esp_netif_init()**: Initialize TCP/IP stack
   - Must be called before any network operation
   - Sets up internal data structures

2. **esp_event_loop_create_default()**: Event system
   - WiFi events (connected, disconnected)
   - IP events (got IP, lost IP)
   - Custom events

3. **esp_netif_create_default_wifi_ap()**: Network interface
   - Creates `192.168.4.1` IP address
   - Sets up DHCP server (assigns IPs to clients)
   - Default subnet mask: `255.255.255.0`

4. **wifi_init()**: Initialize WiFi hardware
   - Allocate buffers
   - Start WiFi task
   - Power on WiFi radio

5. **Event handler**: Callback for WiFi events
   ```c
   void handler(void* arg, esp_event_base_t event_base, 
                int32_t event_id, void* event_data) {
       if (event_id == WIFI_EVENT_AP_STACONNECTED) {
           ESP_LOGI(TAG, "Station connected");
       }
   }
   ```

6. **Configuration**:
   - `ssid`: Network name (visible to users)
   - `password`: Must be 8-64 characters (or blank for open)
   - `max_connection`: Max simultaneous clients (1-4)
   - `authmode`: Security (WPA2 recommended)

7. **Set mode**: `WIFI_MODE_AP` activates Access Point

8. **Start**: Begin broadcasting SSID

### Default IP Configuration

When you create AP:
```
ESP32 IP:      192.168.4.1      (Gateway)
Subnet:        255.255.255.0
DHCP Range:    192.168.4.2 - 192.168.4.10
```

Clients get IPs: 192.168.4.2, 192.168.4.3, etc.

---

## Questions to Reinforce Learning

1. **Why separate the sensor driver into a component?**
   <details>
   <summary>Answer</summary>
   - Reusability across projects
   - Clear dependency management
   - Easier testing
   - Can be shared as library
   </details>

2. **What happens if a FreeRTOS task doesn't call vTaskDelay()?**
   <details>
   <summary>Answer</summary>
   - Task hogs CPU
   - Other tasks can't run (if same/lower priority)
   - Watchdog timer may trigger reset
   - System becomes unresponsive
   </details>

3. **Why do we need `last_update_ms` in our state structure?**
   <details>
   <summary>Answer</summary>
   - Detect if sensor stopped responding
   - Calculate data freshness
   - Debug timing issues
   - Show "stale data" warning in UI
   </details>

4. **What's the difference between httpd_resp_sendstr() and httpd_resp_send_chunk()?**
   <details>
   <summary>Answer</summary>
   - sendstr(): Sends entire response at once, closes connection
   - send_chunk(): Streams data in pieces, keeps connection open
   - Use chunks for: Large files, real-time data, progressive rendering
   </details>

5. **Why use Access Point mode instead of Station mode?**
   <details>
   <summary>Answer</summary>
   - Works without existing WiFi
   - Direct connection (lower latency)
   - No router configuration needed
   - Portable (works anywhere)
   - Trade-off: No internet access
   </details>

---

## Next Steps

In **Part 4**, we'll:
1. Build the complete main.c application
2. Implement the HTTP handlers
3. Create a simple test HTML page
4. Flash and test the system

But first, **exercise**:
- Create the project structure
- Fill in the CMakeLists.txt files
- Configure sdkconfig.defaults
- Try `idf.py build` (it will fail - that's OK!)
- Understand the error messages

**Ready when you are!** Let me know what questions you have about these concepts.
