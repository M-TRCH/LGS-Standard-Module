# Hardware Version Note
**แพลตฟอร์ม:** STM32F103  
**ไฟล์:** LGS-Standard-Module Hardware  

## R4.2 (2025-12-17)
### Compatibility
- Firmware v2.0.2+
- Firmware v2.0.1
- Firmware v2.0.0

### New Features
- เพิ่มเซนเซอร์วัดอุณหภูมิ Built-in Temperature (STS4x)
- ปรับปรุง PCB Layout สำหรับการระายความร้อนที่ดีขึ้น
- ปรับปรุงเสถียรภาพของวงจรเสริม (Decoupling Capacitors)

### Improvements
- ปรับจูนค่าความต้านทานสำหรับการควบคุม LED
- เพิ่มการป้องกัน ESD สำหรับ Modbus interface
- ปรับปรุงการออกแบบ trace บนแผ่น PCB เพื่อลดสัญญาณรบกวน

### Hardware Specs
- Microcontroller: STM32F103C8T6
- Flash Memory: 64 KB
- SRAM: 20 KB
- EEPROM: Built-in (flash-based)
- LEDs: 8 × RGB (WS2812B compatible)
- Communication: RS-485 (Modbus RTU)
- Operating Voltage: 5V / 3.3V Logic
- Max Current: 500mA (per 4 LEDs with full brightness)

---

## R4.1 (2025-11-21)
### Compatibility
- Firmware v2.0.0
- Firmware v1.1.0

### New Features
- เพิ่ม GPIO pins สำหรับสัญญาณขยาย (Expansion pins)
- รองรับการปรับปรุง firmware OTA (Over-The-Air)

### Improvements
- ปรับปรุง Power Supply Filtering
- เพิ่มความเสถียรของสัญญาณ Modbus
- ปรับจูนค่า Slew Rate บนสายส่งสัญญาณ

### Hardware Specs
- Microcontroller: STM32F103C8T6
- Flash Memory: 64 KB
- SRAM: 20 KB
- EEPROM: Built-in (flash-based)
- LEDs: 8 × RGB (WS2812B compatible)
- Communication: RS-485 (Modbus RTU)
- Operating Voltage: 5V / 3.3V Logic
- Max Current: 400mA (per 4 LEDs with full brightness)

---

## R4.0.1 (2025-09-23)
### Compatibility
- Firmware v1.1.0 (Stable)

### Features
- Latch Control Circuit พร้อมการป้องกัน Overcurrent
- Built-in RUN Status LED (GPIO Pin)
- Function Switch (3-position selector/button)
- Temperature Compensation Circuit

### Hardware Specs
- Microcontroller: STM32F103C8T6
- Flash Memory: 64 KB
- SRAM: 20 KB
- EEPROM: Built-in (flash-based)
- LEDs: 8 × RGB (WS2812B compatible)
- Communication: RS-485 (Modbus RTU)
- Operating Voltage: 5V / 3.3V Logic
- Max Current: 400mA (per 4 LEDs with full brightness)

### Pin Configuration
| Pin | Function | Type |
|-----|----------|------|
| PA0 | LED_RUN | Output (Digital) |
| PA1 | FUNC_SW | Input (Digital) |
| PA2 | UART3_TX | Serial TX |
| PA3 | UART3_RX | Serial RX |
| PA9 | SDA (I2C) | I2C Data |
| PA10 | SCL (I2C) | I2C Clock |
| PB6 | LED_DIN | Output (Data) |
| PB7 | LED_CLK | Output (Clock) |

---

## R3.0 (Initial Design - 2025-08-28)
### Compatibility
- Firmware v1.0.0
- Firmware v1.0.1

### Initial Features
- 8-Color RGB LED Support
- Modbus RTU Interface
- Electronic Latch Control
- EEPROM Configuration Storage
- Function Mode Selection

### Hardware Specs
- Microcontroller: STM32F103C8T6
- Flash Memory: 64 KB
- SRAM: 20 KB
- EEPROM: Built-in (flash-based)
- LEDs: 8 × RGB (WS2812B compatible)
- Communication: RS-485 (Modbus RTU)
- Operating Voltage: 5V / 3.3V Logic
- Max Current: 300mA (per 4 LEDs with full brightness)

### Known Limitations
- ไม่รองรับ OTA Firmware Update
- จำนวน GPIO pins จำกัด
- ไม่มีตัวแปลง USB ในบอร์ด

---

## Version Numbering Scheme

**Format: R[Major].[Minor]**

- **Major**: Hardware revision หลัก (เปลี่ยนเมื่อมีการเปลี่ยนแปลงวงจรสำคัญ)
- **Minor**: Hardware revision รอง (เปลี่ยนเมื่อมีการปรับปรุงเล็กน้อย)

### Compatibility Notes
- Firmware เก่า อาจไม่ทำงานได้กับ Hardware ใหม่ (ต้องอัปเดต Firmware)
- Hardware เก่า อาจใช้ Firmware ใหม่ได้บ้าง (ขึ้นอยู่กับความเข้ากันได้)
- ตรวจสอบไฟล์ firmware_version_note.md เพื่อดูรายละเอียดความเข้ากันได้

---

## Hardware Support & Specifications

### Supported Features by Revision

| Feature | R3.0 | R4.0.1 | R4.1 | R4.2 |
|---------|------|--------|------|------|
| 8-Color RGB LEDs | ✓ | ✓ | ✓ | ✓ |
| Modbus RTU | ✓ | ✓ | ✓ | ✓ |
| Latch Control | ✓ | ✓ | ✓ | ✓ |
| Function Switch | ✓ | ✓ | ✓ | ✓ |
| RUN Status LED | ✗ | ✓ | ✓ | ✓ |
| Temp Sensor | ✗ | ✗ | ✗ | ✓ |
| OTA Support | ✗ | ✗ | ✓ | ✓ |
| ESD Protection | ✗ | ✗ | ✓ | ✓ |
| Enhanced Cooling | ✗ | ✗ | ✗ | ✓ |

### Current Consumption (Typical @ 5V)

| Mode | Current | Notes |
|------|---------|-------|
| Idle | 50 mA | Microcontroller + Latch circuit |
| RUN (LEDs off) | 80 mA | With status LED blinking |
| RUN (1 LED @ 50%) | 200 mA | Single LED at 50% brightness |
| RUN (All LEDs @ 100%) | 500 mA | All 8 LEDs at full brightness |
| Latch Unlock | 300 mA | During solenoid activation |
