# Logging System Documentation

## Overview
ระบบ Logging ที่ปรับปรุงใหม่ มีการแบ่งระดับ (Level) และหมวดหมู่ (Category) เพื่อความยืดหยุ่นและมาตรฐานที่ดีขึ้น

## Log Levels
ระดับความสำคัญของข้อความ log (จากน้อยไปมาก):

| Level | Value | Description | Use Case |
|-------|-------|-------------|----------|
| `LOG_NONE` | 0 | ไม่แสดง log ใดๆ | Production mode |
| `LOG_ERROR` | 1 | แสดงข้อผิดพลาดร้ายแรง | ข้อผิดพลาดที่ทำให้ระบบไม่สามารถทำงานได้ |
| `LOG_WARNING` | 2 | แสดงคำเตือน + ข้อผิดพลาด | สถานการณ์ผิดปกติที่ควรตรวจสอบ |
| `LOG_INFO` | 3 | แสดงข้อมูลทั่วไป + คำเตือน + ข้อผิดพลาด | ข้อมูลการทำงานปกติของระบบ (Default) |
| `LOG_DEBUG` | 4 | แสดงข้อมูล Debug + ทุกอย่างข้างต้น | การ Debug ระหว่างพัฒนา |
| `LOG_VERBOSE` | 5 | แสดงข้อมูลละเอียดทั้งหมด | การ Debug แบบละเอียดมาก |

## Log Categories
หมวดหมู่ของ log ตามโมดูลการทำงาน (ใช้ bit flags):

| Category | Value | Description |
|----------|-------|-------------|
| `LOG_CAT_NONE` | 0x00 | ไม่แสดงหมวดหมู่ใดๆ |
| `LOG_CAT_SYSTEM` | 0x01 | ระบบทั่วไป (initialization, reset, etc.) |
| `LOG_CAT_EEPROM` | 0x02 | การจัดการ EEPROM (read, write, save) |
| `LOG_CAT_MODBUS` | 0x04 | การสื่อสาร Modbus |
| `LOG_CAT_LED` | 0x08 | การควบคุม LED |
| `LOG_CAT_ALL` | 0xFF | แสดงทุกหมวดหมู่ (Default) |

## Configuration
ตั้งค่าระดับและหมวดหมู่ที่ต้องการแสดงใน `system.cpp`:

```cpp
// ตั้งระดับ log ทั่วไป
LogLevel globalLogLevel = LOG_INFO;  // แสดง INFO, WARNING, ERROR

// เปิดหมวดหมู่ที่ต้องการ (ใช้ bitwise OR เพื่อรวมหลายหมวดหมู่)
uint8_t enabledLogCategories = LOG_CAT_ALL;                    // ทุกหมวดหมู่
// หรือ
uint8_t enabledLogCategories = LOG_CAT_SYSTEM | LOG_CAT_LED;  // เฉพาะ SYSTEM และ LED
```

## Usage Examples

### ตัวอย่างการใช้งานพื้นฐาน

```cpp
// System logs
LOG_ERROR_SYS(F("[SYSTEM] Critical error occurred!\n"));
LOG_WARNING_SYS(F("[SYSTEM] Warning: Low memory\n"));
LOG_INFO_SYS(F("[SYSTEM] Initialization complete\n"));
LOG_DEBUG_SYS(F("[SYSTEM] Debug info\n"));
LOG_VERBOSE_SYS(F("[SYSTEM] Verbose details\n"));

// EEPROM logs
LOG_INFO_EEPROM(F("[EEPROM] Configuration loaded\n"));
LOG_ERROR_EEPROM(F("[EEPROM] Read failed!\n"));

// Modbus logs
LOG_INFO_MODBUS("[MODBUS] ID: " + String(id) + "\n");
LOG_WARNING_MODBUS(F("[MODBUS] Timeout detected\n"));

// LED logs
LOG_INFO_LED(F("[LED] Initialization complete\n"));
LOG_DEBUG_LED("[LED] LED" + String(i) + " turned ON\n");
LOG_WARNING_LED("[LED] LED" + String(i) + " max time exceeded\n");
```

### Generic Log Macro
สำหรับกรณีที่ต้องการควบคุมทั้ง level และ category เอง:

```cpp
LOG(LOG_INFO, LOG_CAT_SYSTEM, F("Custom message\n"));
```

## Best Practices

1. **ใช้ F() macro กับ string literals**
   ```cpp
   LOG_INFO_SYS(F("[SYSTEM] Static text\n"));  // ดี - ประหยัด RAM
   ```

2. **ใส่ prefix ระบุหมวดหมู่**
   ```cpp
   LOG_INFO_LED(F("[LED] Message\n"));         // ดี - อ่านง่าย
   ```

3. **เลือก log level ที่เหมาะสม**
   - `ERROR` - ข้อผิดพลาดร้ายแรงเท่านั้น
   - `WARNING` - สถานการณ์ผิดปกติที่ควรตรวจสอบ
   - `INFO` - ข้อมูลการทำงานปกติ
   - `DEBUG` - ข้อมูลสำหรับ debug
   - `VERBOSE` - รายละเอียดทั้งหมด

4. **ควบคุมหมวดหมู่ใน production**
   ```cpp
   // Production: แสดงเฉพาะ ERROR และ WARNING ของ SYSTEM และ MODBUS
   globalLogLevel = LOG_WARNING;
   enabledLogCategories = LOG_CAT_SYSTEM | LOG_CAT_MODBUS;
   ```

5. **ปิด logging ใน production**
   ```cpp
   globalLogLevel = LOG_NONE;  // ปิดทั้งหมด
   ```

## Performance Considerations

- Log macros ใช้ `do-while(0)` pattern เพื่อความปลอดภัย
- ตรวจสอบ level และ category ก่อนแสดงผล (no overhead ถ้าไม่แสดง)
- ใช้ F() macro เพื่อประหยัด RAM

## Summary

ระบบ logging ใหม่นี้ให้:
- ✅ แบ่งระดับความสำคัญชัดเจน (6 levels)
- ✅ แบ่งหมวดหมู่ตามโมดูล (4+ categories)
- ✅ ควบคุมได้ง่ายและยืดหยุ่น
- ✅ Performance overhead ต่ำ
- ✅ มาตรฐานและอ่านง่าย
