# MCP9808 Temperature Sensor Tutorial - Part 1: Understanding the Hardware

A hands-on educational project to learn ESP32 driver development, I2C communication, and web UI integration.

## Project Overview

**What We're Building**:
- Read temperature from MCP9808 sensor via I2C
- Display temperature on web interface
- Show real-time temperature chart
- Add placeholder buttons for control

**What You'll Learn**:
1. I2C protocol and communication
2. Reading sensor datasheets
3. Writing ESP32 drivers
4. FreeRTOS task management
5. HTTP API development
6. Angular component creation
7. Real-time data visualization

## About the MCP9808

### Specifications

- **Type**: Digital temperature sensor
- **Interface**: I2C (Two Wire Interface)
- **Resolution**: 0.0625°C (±0.25°C accuracy)
- **Range**: -40°C to +125°C
- **Supply Voltage**: 2.7V to 5.5V
- **I2C Address**: 0x18 (default, configurable to 0x18-0x1F)

### Why MCP9808?

1. **Simple**: Only needs 4 wires (VCC, GND, SDA, SCL)
2. **Accurate**: High precision temperature readings
3. **Common**: Readily available, well documented
4. **I2C**: Standard protocol, easy to learn

## Hardware Connection

### ESP32-S3 I2C Pins

Your ESP32-S3-WROOM-1 has I2C available on:
- **SDA (Data)**: GPIO 47
- **SCL (Clock)**: GPIO 48

These are already configured in ESP-Miner!

### Wiring Diagram

```
MCP9808                    ESP32-S3
────────                   ────────
VCC (Pin 8) ─────────────> 3.3V
GND (Pin 4) ─────────────> GND
SDA (Pin 2) ─────────────> GPIO 47 (SDA)
SCL (Pin 6) ─────────────> GPIO 48 (SCL)
```

**Pin Layout** (Top View):
```
MCP9808 MSOP-8
    ┌─────────┐
SDA │1  •   8│ VCC
    │         │
SCL │2      7│ A2
    │         │
A0  │3      6│ A1
    │         │
GND │4      5│ Alert
    └─────────┘
```

### I2C Bus Topology

```
         ESP32-S3
            │
            ├─── SDA ───┐
            │           │
            ├─── SCL ───┤
            │           │
         ┌──┴──┐     ┌──┴──┐
         │ R   │     │ R   │   R = Pull-up Resistor
         │4.7kΩ│     │4.7kΩ│       (usually built-in)
         └──┬──┘     └──┬──┘
            │           │
         To VCC      To VCC
            │           │
       ┌────┴───────────┴────┐
       │                     │
   ┌───┴───┐             ┌───┴───┐
   │MCP9808│             │ Other │
   │ 0x18  │             │Device │
   └───────┘             └───────┘
```

**Note**: I2C requires pull-up resistors on SDA and SCL lines. Your ESP32 development board likely has these built-in.

## Understanding I2C Protocol

### What is I2C?

**I2C** (Inter-Integrated Circuit) is a synchronous serial protocol:
- **Two wires**: SDA (data) and SCL (clock)
- **Multi-master**: Multiple devices can control the bus
- **Multi-slave**: Up to 128 devices on one bus
- **Addressable**: Each device has a unique 7-bit address

### I2C Transaction

```
Master (ESP32) sends:
START → ADDRESS → R/W → ACK → DATA → ACK → STOP

Example: Read temperature from MCP9808 (address 0x18):

1. START condition
2. Send 0x18 (device address) + WRITE bit
3. Wait for ACK from MCP9808
4. Send 0x05 (register address - temperature)
5. Wait for ACK
6. REPEATED START
7. Send 0x18 + READ bit
8. Wait for ACK
9. Read 2 bytes (temperature data)
10. Send NACK (last byte)
11. STOP condition
```

### Visualising I2C Signals

```
SDA:  ─┐  ┌──┐  ┌─┐  ┌─┐  ┌─────┐  ┌─────────
      │  │  │  │ │  │ │  │     │  │
      └──┘  └──┘ └─┘  └─┘  └─────┘  └─────────
       S  A6 A5 A4 A3 A2 A1 A0  W/R  ACK  DATA...

SCL:  ───┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─
         │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
         └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └

Legend:
S = START condition (SDA falls while SCL high)
W/R = 0 for Write, 1 for Read
ACK = Acknowledge (SDA low)
```

## Reading the MCP9808 Datasheet

**Download**: [MCP9808 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/25095A.pdf)

### Key Information We Need

#### 1. I2C Address (Page 14)

```
Default: 0x18 (binary: 0011000)

Can be changed using A0, A1, A2 pins:
A2 A1 A0 | Address
0  0  0  | 0x18 (default)
0  0  1  | 0x19
0  1  0  | 0x1A
... up to 0x1F
```

#### 2. Register Map (Page 15)

```
Register | Name              | Address | Size
---------|-------------------|---------|------
0x05     | Temperature       | 0x05    | 16-bit
0x01     | Configuration     | 0x01    | 16-bit
0x06     | Manufacturer ID   | 0x06    | 16-bit
0x07     | Device ID         | 0x07    | 16-bit
```

#### 3. Temperature Register Format (Page 19)

```
15 14 13 12 | 11 10  9  8 | 7  6  5  4 | 3  2  1  0
─────────────────────────────────────────────────────
C  A  A  A  | 2^7 ... 2^4 | 2^3 ... 2^0
│  │  │  │
│  │  │  └─ Alert flags
│  └──┴──── Temperature sign/alerts
└────────── Critical flag

Temperature bits: 12:0 (13 bits total)
- Bit 12: Sign (1 = negative)
- Bits 11-0: Temperature value

Resolution: 0.0625°C per LSB
```

#### 4. Calculation (Page 19)

**Positive Temperature**:
```
Temperature = (Byte1 × 16) + (Byte0 / 16)
```

**Example**:
```
Register value: 0x0191 (binary: 0000 0001 1001 0001)

Upper byte: 0x01 = 1
Lower byte: 0x91 = 145

Temperature = (1 × 16) + (145 / 16)
            = 16 + 9.0625
            = 25.0625°C
```

**Negative Temperature** (2's complement):
```
Register value: 0x1FC9 (binary: 0001 1111 1100 1001)

Bit 12 is 1, so negative
Mask off upper bits: 0x0FC9 = 4041
Convert from 2's complement: 4096 - 4041 = 55
Temperature = -(55 × 0.0625) = -3.4375°C
```

## Verification Steps

Before we start coding, let's verify the hardware:

### Step 1: Check Wiring

Use a multimeter to verify:
1. **VCC to ESP32 3.3V**: Should read ~3.3V
2. **GND to ESP32 GND**: Should read 0V (continuity)
3. **SDA to GPIO 47**: Continuity check
4. **SCL to GPIO 48**: Continuity check

### Step 2: I2C Bus Scan

We'll write a simple programme to scan the I2C bus and detect the MCP9808:

```c
// This will be our first code example
// We'll implement this in Part 2
void i2c_scanner(void) {
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_probe(addr) == ESP_OK) {
            printf("Found device at 0x%02X\n", addr);
        }
    }
}

// Expected output:
// Found device at 0x18
```

## Next Steps

In **Part 2**, we'll:
1. Set up the project structure
2. Create the MCP9808 driver component
3. Implement I2C communication
4. Read temperature values
5. Test with serial output

---

## Homework Before Part 2

1. **Wire up your MCP9808** to the ESP32-S3
2. **Read sections 5.0 and 5.1** of the MCP9808 datasheet
3. **Review the I2C protocol** - understand START, STOP, ACK, NACK

## Questions to Think About

1. Why does I2C need pull-up resistors?
2. What happens if two devices have the same I2C address?
3. How would you handle a sensor that doesn't respond?
4. Why is temperature stored as 2 bytes instead of 1?

---

**Ready for Part 2?** Let me know when you want to continue!
