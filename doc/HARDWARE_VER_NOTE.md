# Hardware Version Note

## R4.2 (High Volume - 2025-12-22)

### Improvements
- ใส่ TVS Diode ป้องกันแรงดันขาเข้ากระชาก  
- ใส่ไดโอดก่อนเข้า NPN Mosfet ป้องกันกลอนไฟฟ้าทำงานเมื่อจ่ายไฟกลับขั้ว
- เปลี่ยนตัวเก็บประจุคร่อม LDO เป็นแบบ Electrolytes

---

## R4.2 (2025-11-13)

### Improvements
- เปลี่ยนตัวเก็บประจุคร่อม LDO ให้มีขนาดเล็กลง 

---

## R4.1 (New Layout - 2025-10-18)

### New Features
- เพิ่มช่องต่อจอ OLED 0.96"
- ใส่ Function Switch แทน Row, Column 

### Improvements
- ปรับ Components Layout ให้รองรับการต่อพ่วงจำนวนมาก 
- Operating Voltage up to 24V
- เปลี่ยนช่องต่อกลอนไฟฟ้าเป็นขั้ว JST-XH 4P

---

## R4.0.1 (First Modbus RTU - 2025-08-27)

### Improvements
- PCB Layout เดิมจาก R4.0
- เปลี่ยน MCU เป็น STM32F103C8T6
- ใช้การสื่อสาร Modbus RTU แทน Custom Protocol

### Hardware Specs
- Microcontroller: STM32F103C8T6 (Reduced to 24MHz)
- Flash Memory: 64 KB
- SRAM: 20 KB
- EEPROM: Built-in (flash-based)
- LEDs: 8 × RGB (WS2812B-MINI-X2)
- Communication: RS-485 (Modbus RTU)
- Operating Voltage: 5V < Input < 12V

---

## R4.0 (LGS 8-Color Initial Design - 2024-12-12)

### Initial Features
- 8-Color RGB LED Support
- Modbus RTU Interface
- Electronic Latch Control
- EEPROM Configuration Storage
- Row and Column Button

### Hardware Specs
- Microcontroller: STM32F030C8T6
- Flash Memory: 64 KB
- SRAM: 8 KB
- EEPROM: Built-in (flash-based)
- LEDs: 8 × RGB (WS2812B-MINI-X2)
- Communication: RS-485 (Custom protocol)
- Operating Voltage: 5V < Input < 12V
---

## Version Numbering Scheme

### Format: R[Major].[Minor]

- **Major**: Hardware revision หลัก (เปลี่ยนเมื่อมีการเปลี่ยนแปลงวงจรสำคัญ)
- **Minor**: Hardware revision รอง (เปลี่ยนเมื่อมีการปรับปรุงเล็กน้อย)

### Compatibility Notes
- Firmware เก่าไม่สามารถใช้ได้กับ Hardware ใหม่ได้
- Hardware เก่าบางรุ่นสามารถใช้ Firmware ใหม่ได้ (ตรวจสอบใน firmware_version_note.md)

---

## Hardware Support & Specifications

### Supported Features by Revision

| Feature | R4.0 | R4.0.1 | R4.1 | R4.2 | R4.3 |
|---------|------|--------|------|------|------|
| 12V Input Support | ✗ | ✗ | ✓ | ✓ | ✓ |
| 8-Color RGB LEDs | ✓ | ✓ | ✓ | ✓ | ✓ |
| Modbus RTU | ✓ | ✓ | ✓ | ✓ | ✓ |
| Latch Control | ✓ | ✓ | ✓ | ✓ | ✓ |
| Function Switch | ✗ | ✗ | ✓ | ✓ | ✓ |
| RUN Status LED | ✗ | ✗ | ✓ | ✓ | ✓ |
| Temp Sensor | ✗ | ✗ | ✓ | ✓ | ✓ |
| Surge Protection | ✗ | ✗ | ✗ | ✗ | ✓ |

### Current Consumption (Typical @ 12V)

| Mode | Current | Notes |
|------|---------|-------|
| Idle | N/A | Microcontroller + Latch circuit |
| RUN (LEDs off) | N/A | With status LED blinking |
| RUN (1 LED @ 80%) | N/A | Single LED at 80% brightness |
| RUN (All LEDs @ 80%) | N/A | All 8 LEDs at 80% brightness |
| Small Latch Unlock | 300 mA | During solenoid activation @300ms|
| Large Latch Unlock | 2,000 mA | During solenoid activation @500ms|
