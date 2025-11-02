# Function Switch (FUNC_SW_PIN) Usage Guide

## Overview
ระบบตรวจสอบสวิตช์ฟังก์ชัน (Function Switch) ขณะบอร์ดเริ่มทำงาน เพื่อเข้าสู่โหมดพิเศษต่างๆ โดยการกดค้างสวิตช์เป็นระยะเวลาที่กำหนด

## Hardware
- **Pin**: `FUNC_SW_PIN` (PA0)
- **Type**: Active LOW (กดแล้วเป็น LOW)
- **Connection**: สวิตช์ต่อระหว่าง PA0 และ GND

## Function Switch Modes

| Mode | Press Duration | Enum Value | Use Case |
|------|---------------|------------|----------|
| **NONE** | ไม่กด หรือ < 1 วินาที | `FUNC_SW_NONE` (0) | ทำงานปกติ |
| **SHORT** | กดค้าง > 1 วินาที | `FUNC_SW_SHORT` (1) | Diagnostic Mode |
| **MEDIUM** | กดค้าง > 5 วินาที | `FUNC_SW_MEDIUM` (2) | Factory Reset (ยกเว้น ID) |
| **LONG** | กดค้าง > 10 วินาที | `FUNC_SW_LONG` (3) | Complete Factory Reset |

## API Reference

### Function: `checkFunctionSwitch()`

```cpp
FunctionSwitchMode checkFunctionSwitch(uint16_t maxWaitTime = 15000);
```

**Parameters:**
- `maxWaitTime` - เวลาสูงสุดที่รอการกดสวิตช์ (default: 15000ms = 15 วินาที)

**Returns:**
- `FunctionSwitchMode` enum value (0-3)

**Description:**
- ตรวจสอบสวิตช์ฟังก์ชันขณะเริ่มระบบ
- วัดระยะเวลาที่กดค้าง
- แสดง log และ LED feedback
- คืนค่าโหมดที่ตรวจพบ

### Enum: `FunctionSwitchMode`

```cpp
enum FunctionSwitchMode
{
    FUNC_SW_NONE = 0,      // ไม่กด
    FUNC_SW_SHORT = 1,     // > 1 วินาที
    FUNC_SW_MEDIUM = 2,    // > 5 วินาที
    FUNC_SW_LONG = 3       // > 10 วินาті
};
```

## Usage Example

### Basic Usage

```cpp
void setup() 
{
    sysInit(LOG_DEBUG);
    
    // Check function switch at startup
    FunctionSwitchMode mode = checkFunctionSwitch();
    
    switch (mode) 
    {
        case FUNC_SW_SHORT:
            Serial.println("SHORT press detected");
            // Do something for short press
            break;
            
        case FUNC_SW_MEDIUM:
            Serial.println("MEDIUM press detected");
            // Do something for medium press
            break;
            
        case FUNC_SW_LONG:
            Serial.println("LONG press detected");
            // Do something for long press
            break;
            
        case FUNC_SW_NONE:
        default:
            Serial.println("Normal operation");
            break;
    }
    
    // Continue with normal setup...
}
```

### Advanced Example (with Different Actions)

```cpp
void setup() 
{
    sysInit(LOG_DEBUG);
    
    FunctionSwitchMode mode = checkFunctionSwitch();
    
    switch (mode) 
    {
        case FUNC_SW_SHORT:
            enterDiagnosticMode();
            break;
            
        case FUNC_SW_MEDIUM:
            loadEepromConfig();
            factoryResetExceptID();
            NVIC_SystemReset();  // Will not return
            break;
            
        case FUNC_SW_LONG:
            loadEepromConfig();
            completeFactoryReset();
            NVIC_SystemReset();  // Will not return
            break;
            
        case FUNC_SW_NONE:
        default:
            // Normal operation
            handleFirstBoot();
            loadEepromConfig();
            break;
    }
    
    // Continue with rest of setup...
    ledInit();
    modbusInit(eepromConfig.identifier);
}

void enterDiagnosticMode() 
{
    LOG_INFO_SYS(F("[SYSTEM] Diagnostic mode\n"));
    // Test all LEDs
    for (int i = 0; i < LED_NUM; i++) 
    {
        leds[i]->setPixelColor(0, leds[i]->Color(255, 255, 255));
        leds[i]->show();
        delay(200);
        leds[i]->clear();
        leds[i]->show();
    }
}

void factoryResetExceptID() 
{
    LOG_INFO_SYS(F("[SYSTEM] Factory reset (except ID)\n"));
    eepromConfig.isFirstBootExceptID = true;
    saveEepromConfig();
}

void completeFactoryReset() 
{
    LOG_INFO_SYS(F("[SYSTEM] Complete factory reset\n"));
    eepromConfig.isFirstBoot = true;
    saveEepromConfig();
}
```

## LED Feedback

ระหว่างการกดสวิตช์ LED_RUN_PIN จะกระพริบเพื่อแสดงโหมด:

### ขณะกดค้าง:
- **1-4 วินาที**: กระพริบช้า (500ms) - SHORT mode
- **5-9 วินาที**: ติดค้าง - MEDIUM mode
- **10+ วินาที**: กระพริบเร็ว (1s) - LONG mode

### หลังปล่อยสวิตช์:
- กระพริบตามจำนวนโหมด
  - SHORT (1): กระพริบ 1 ครั้ง
  - MEDIUM (2): กระพริบ 2 ครั้ง
  - LONG (3): กระพริบ 3 ครั้ง

## Serial Log Output

### Normal Operation (ไม่กด):
```
[SYSTEM] Checking function switch...
[SYSTEM] Function switch not pressed, continuing normal operation
```

### SHORT Press (>1s):
```
[SYSTEM] Checking function switch...
[SYSTEM] Function switch detected! Waiting for release...
[SYSTEM] Switch pressed: 1 seconds...
[SYSTEM] Switch pressed: 2 seconds...
[SYSTEM] Function switch: SHORT press (>1s) detected
```

### MEDIUM Press (>5s):
```
[SYSTEM] Checking function switch...
[SYSTEM] Function switch detected! Waiting for release...
[SYSTEM] Switch pressed: 1 seconds...
[SYSTEM] Switch pressed: 2 seconds...
[SYSTEM] Switch pressed: 3 seconds...
[SYSTEM] Switch pressed: 4 seconds...
[SYSTEM] Switch pressed: 5 seconds...
[SYSTEM] Switch pressed: 6 seconds...
[SYSTEM] Function switch: MEDIUM press (>5s) detected
```

### LONG Press (>10s):
```
[SYSTEM] Checking function switch...
[SYSTEM] Function switch detected! Waiting for release...
[SYSTEM] Switch pressed: 1 seconds...
...
[SYSTEM] Switch pressed: 10 seconds...
[SYSTEM] Switch pressed: 11 seconds...
[SYSTEM] Function switch: LONG press (>10s) detected
```

## Timing Details

| Event | Time (ms) | Description |
|-------|-----------|-------------|
| Short threshold | 1,000 | ต้องกดค้างอย่างน้อย 1 วินาที |
| Medium threshold | 5,000 | ต้องกดค้างอย่างน้อย 5 วินาที |
| Long threshold | 10,000 | ต้องกดค้างอย่างน้อย 10 วินาที |
| Max wait time | 15,000 | รอสูงสุด 15 วินาที (ปรับได้) |
| Debounce delay | 100 | หน่วงหลังปล่อยสวิตช์ |
| Check interval | 50 | ตรวจสอบสถานะทุก 50ms |

## Use Cases

### 1. Diagnostic Mode (SHORT press - >1s)
**Purpose:** ทดสอบฮาร์ดแวร์โดยไม่ต้องรีเซ็ต

**Example:**
- ทดสอบ LED ทั้งหมด
- ทดสอบเซนเซอร์
- แสดงข้อมูลระบบ
- ทดสอบ Modbus communication

```cpp
case FUNC_SW_SHORT:
    testAllLEDs();
    testSensor();
    displaySystemInfo();
    // Continue normal operation after tests
    break;
```

### 2. Factory Reset Except ID (MEDIUM press - >5s)
**Purpose:** รีเซ็ตค่าทั้งหมดยกเว้น Modbus ID

**Example:**
```cpp
case FUNC_SW_MEDIUM:
    loadEepromConfig();
    uint16_t savedID = eepromConfig.identifier;
    eepromConfig = eepromConfig_default;
    eepromConfig.identifier = savedID;  // Restore ID
    saveEepromConfig();
    NVIC_SystemReset();
    break;
```

### 3. Complete Factory Reset (LONG press - >10s)
**Purpose:** รีเซ็ตทั้งหมดรวม Modbus ID

**Example:**
```cpp
case FUNC_SW_LONG:
    loadEepromConfig();
    eepromConfig = eepromConfig_default;
    saveEepromConfig();
    NVIC_SystemReset();
    break;
```

## Best Practices

### 1. เรียกใช้หลัง `sysInit()`
```cpp
void setup() {
    sysInit(LOG_DEBUG);           // Initialize first
    FunctionSwitchMode mode = checkFunctionSwitch();  // Then check switch
    // ...
}
```

### 2. จัดการ EEPROM ตามโหมด
```cpp
// Don't load EEPROM before checking switch
FunctionSwitchMode mode = checkFunctionSwitch();

// Load EEPROM only when needed
if (mode == FUNC_SW_MEDIUM || mode == FUNC_SW_LONG) {
    loadEepromConfig();
}
```

### 3. ให้ User Feedback
```cpp
// Visual feedback with LEDs
for (int i = 0; i < mode; i++) {
    digitalWrite(LED_RUN_PIN, HIGH);
    delay(200);
    digitalWrite(LED_RUN_PIN, LOW);
    delay(200);
}
```

### 4. Validate Switch Release
```cpp
// Wait for switch to be released
delay(100);
while (digitalRead(FUNC_SW_PIN) == LOW) {
    delay(10);
}
```

## Troubleshooting

### ปัญหา: สวิตช์ไม่ทำงาน
**สาเหตุ:**
- สาย GND ไม่ติด
- สวิตช์เสีย
- Pin ไม่ได้ตั้งเป็น INPUT

**แก้ไข:**
```cpp
// Check pin configuration in sysInit()
pinMode(FUNC_SW_PIN, INPUT);  // Or INPUT_PULLUP
```

### ปัญหา: ตรวจจับผิดโหมด
**สาเหตุ:**
- ปล่อยสวิตช์เร็วเกินไป
- Bounce ของสวิตช์

**แก้ไข:**
- กดค้างให้นานพอ (>threshold)
- ใช้ hardware debounce circuit
- เพิ่ม software debounce

### ปัญหา: รีเซ็ตบ่อย
**สาเหตุ:**
- สวิตช์ลัดวงจร
- Noise บน pin

**แก้ไข:**
- เพิ่ม pull-up resistor (10kΩ)
- เพิ่ม capacitor (0.1µF) ที่สวิตช์

## Hardware Connection

### Schematic
```
         STM32F103C8T6
              PA0 ────────┐
                          │
                         ─┴─  Function Switch
                         │ │  (SPST, normally open)
                         ─┬─
                          │
                         GND
```

### With Pull-up Resistor (Recommended)
```
         STM32F103C8T6
        3.3V
         │
        ┌┴┐
        │ │ 10kΩ
        └┬┘
         │
         ├────── PA0 (FUNC_SW_PIN)
         │
        ─┴─  Function Switch
        │ │  (SPST)
        ─┬─
         │
        GND
```

## Complete Example

ดูตัวอย่างเต็มได้ที่:
- `examples/main_example_func_switch.cpp`

## Notes

1. **Timing**: การกดค้างต้องเกิน threshold พอดี ถ้าปล่อยก่อนถึง threshold จะไม่ตรวจจับ
2. **Active LOW**: สวิตช์ทำงานแบบ active LOW (กด = LOW, ไม่กด = HIGH)
3. **Blocking**: ฟังก์ชันจะรอจนกว่าจะปล่อยสวิตช์หรือหมดเวลา (blocking)
4. **Reset**: โหมด MEDIUM และ LONG จะรีเซ็ตบอร์ด ดังนั้นโค้ดหลังจาก `NVIC_SystemReset()` จะไม่ทำงาน
5. **Log Level**: ตั้ง log level เป็น `LOG_DEBUG` เพื่อดูข้อมูลละเอียด

## Version History

- **v1.0** (November 2, 2025)
  - Initial implementation
  - Support 3 modes: SHORT, MEDIUM, LONG
  - LED feedback
  - Serial logging
