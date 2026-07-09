# LGS Standard Module — Documentation

เอกสารประกอบเฟิร์มแวร์ตู้ยาไฟนำทาง (Light Guiding Shelf) สำหรับ STM32

## สารบัญ

| เอกสาร | เนื้อหา |
|--------|---------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | โครงสร้างเฟิร์มแวร์, HAL, การเลือกบอร์ด, feature flags |
| [MODBUS_CONTROL_TABLE.md](MODBUS_CONTROL_TABLE.md) | ตาราง Modbus Register/Coil ทั้งหมด |
| [FIRMWARE_UPDATE_GUIDE.md](FIRMWARE_UPDATE_GUIDE.md) | ขั้นตอนการ build และอัพเดท firmware |
| [FIRMWARE_VERSION_NOTE.md](FIRMWARE_VERSION_NOTE.md) | ประวัติเวอร์ชัน firmware |
| [HARDWARE_VERSION_NOTE.md](HARDWARE_VERSION_NOTE.md) | ประวัติเวอร์ชัน hardware และสเปก |

## เริ่มต้นอย่างรวดเร็ว

```bash
# Build บอร์ดปัจจุบัน (STM32F103)
pio run -e genericSTM32F103C8

# Build บอร์ดรุ่นใหม่ (STM32G030)
pio run -e genericSTM32G030C8

# Build พร้อมเปิดจอ OLED
pio run -e genericSTM32F103C8 -a "--project-option=build_flags=-D LGS_ENABLE_DISPLAY=1"
```

## โครงสร้างโปรเจกต์

```
include/
  hw_config.h        Pin mapping แยกตามบอร์ด (F103 / G030)
  modbus_map.h       Register/Coil enum + helper
  system.h           แกนระบบ, logging, function switch
  led.h              HAL: ไฟนำทางแต่ละช่อง
  eeprom_utils.h     การเก็บค่าถาวร
  app.h              ลอจิกการทำงาน
  hal/
    hal_latch.h      HAL: กลไกล็อคลิ้นชัก
    hal_sensor.h     HAL: เซ็นเซอร์อุณหภูมิ
    hal_display.h    HAL: จอ OLED (optional)
src/
  main.cpp           Entry point (บาง)
  app.cpp            ลอจิกการทำงาน
  system.cpp, led.cpp, eeprom_utils.cpp, modbus_utils.cpp
  hal/               การ implement HAL
boards/
  genericSTM32G030C8.json   Board definition สำหรับ STM32G030
```
