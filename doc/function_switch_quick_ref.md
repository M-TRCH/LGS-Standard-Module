# Function Switch Quick Reference

## การใช้งานเบื้องต้น

### 1. เพิ่มในโค้ด
```cpp
#include "system.h"

void setup() {
    sysInit(LOG_DEBUG);
    
    // ตรวจสอบสวิตช์ฟังก์ชัน
    FunctionSwitchMode mode = checkFunctionSwitch();
    
    switch (mode) {
        case FUNC_SW_SHORT:   // กด > 1 วินาที
            // ทำอะไรบางอย่าง
            break;
            
        case FUNC_SW_MEDIUM:  // กด > 5 วินาที
            // Factory reset (ยกเว้น ID)
            break;
            
        case FUNC_SW_LONG:    // กด > 10 วินาที
            // Complete factory reset
            break;
            
        case FUNC_SW_NONE:    // ไม่กด
        default:
            // ทำงานปกติ
            break;
    }
}
```

### 2. โหมดที่รองรับ

| โหมด | ระยะเวลา | Enum | ค่า |
|------|----------|------|-----|
| ไม่กด | < 1s | `FUNC_SW_NONE` | 0 |
| สั้น | > 1s | `FUNC_SW_SHORT` | 1 |
| ปานกลาง | > 5s | `FUNC_SW_MEDIUM` | 2 |
| ยาว | > 10s | `FUNC_SW_LONG` | 3 |

### 3. การต่อสาย
```
PA0 (FUNC_SW_PIN) ───┬──── Switch ──── GND
                     │
                    10kΩ (optional pull-up)
                     │
                    3.3V
```

### 4. LED Feedback
- **กดค้าง 1-4s**: กระพริบช้า → SHORT mode
- **กดค้าง 5-9s**: ติดค้าง → MEDIUM mode  
- **กดค้าง 10+s**: กระพริบเร็ว → LONG mode
- **หลังปล่อย**: กระพริบตามจำนวนโหมด (1-3 ครั้ง)

## ตัวอย่างการใช้งานจริง

### Diagnostic Mode (SHORT)
```cpp
case FUNC_SW_SHORT:
    LOG_INFO_SYS(F("Entering diagnostic mode\n"));
    // Test LEDs
    for (int i = 0; i < LED_NUM; i++) {
        leds[i]->setPixelColor(0, leds[i]->Color(255, 255, 255));
        leds[i]->show();
        delay(200);
        leds[i]->clear();
        leds[i]->show();
    }
    break;
```

### Factory Reset Except ID (MEDIUM)
```cpp
case FUNC_SW_MEDIUM:
    loadEepromConfig();
    eepromConfig.isFirstBootExceptID = true;
    saveEepromConfig();
    NVIC_SystemReset();  // Restart
    break;
```

### Complete Factory Reset (LONG)
```cpp
case FUNC_SW_LONG:
    loadEepromConfig();
    eepromConfig.isFirstBoot = true;
    saveEepromConfig();
    NVIC_SystemReset();  // Restart
    break;
```

## หมายเหตุสำคัญ

1. ✅ เรียก `sysInit()` ก่อนเสมอ
2. ✅ สวิตช์เป็น **Active LOW** (กด = LOW)
3. ⚠️ ฟังก์ชันจะ **block** จนกว่าปล่อยสวิตช์
4. ⚠️ โหมด MEDIUM/LONG จะ **รีเซ็ตบอร์ด** ทันที

## ไฟล์ที่เกี่ยวข้อง

- **Implementation**: `src/system.cpp` - ฟังก์ชัน `checkFunctionSwitch()`
- **Header**: `include/system.h` - enum และ prototype
- **Example**: `examples/main_example_func_switch.cpp` - ตัวอย่างเต็ม
- **Documentation**: `doc/function_switch_usage.md` - คู่มือละเอียด

## API

```cpp
FunctionSwitchMode checkFunctionSwitch(uint16_t maxWaitTime = 15000);
```

**Parameters:**
- `maxWaitTime` - เวลารอสูงสุด (default 15000ms)

**Returns:**
- `FUNC_SW_NONE` (0) - ไม่กด
- `FUNC_SW_SHORT` (1) - กด > 1s
- `FUNC_SW_MEDIUM` (2) - กด > 5s  
- `FUNC_SW_LONG` (3) - กด > 10s

---
**Version**: 1.0  
**Date**: November 2, 2025  
**Pin**: PA0 (FUNC_SW_PIN)
