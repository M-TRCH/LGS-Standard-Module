# Hardware Version Note

## R5.1 (LGS-Standalone - 2026-08)

### New Features
- ปุ่ม **SW3 บน PC13** (tactile + pull-up 10k + 100nF) — **ตัวแทนของ function switch เดิม (KEY1/PA7)** สำหรับตู้ที่หน้ากากบังปุ่มใหญ่ · firmware อ่านสองปุ่มเป็นสวิตช์เดียว (OR) ตั้งแต่ v3.2.0
- schematic แผ่น **MCU** และ **LED & OLED** ขยับเป็น rev 5.1 · แผ่น RS485 / Servo-Latch / Switching คงเดิมที่ rev 5.0

### Notes
- วงจรวัดกระแสขาเข้า INA180A4 (×200, shunt 3 mΩ → PA6) **มีมาตั้งแต่ R5.0** — firmware ≥ v3.2.0 เริ่มอ่านและรายงานที่ reg 22 (mA)
- **firmware image เดียวใช้ทั้ง R5.0 และ R5.1** — PC13 บน R5.0 ไม่เดินสาย firmware เปิด internal pull-up ให้เงียบ · reg 2 รายงาน **510 บนทุกบอร์ด** เพราะ R5.0 ไม่เข้าสายการผลิตจริง

---

## R5.0 (STM32G0 - 2026)

### New Features
- เปลี่ยน MCU เป็น STM32G070CBT6 (64MHz, Flash 128KB, SRAM 36KB)
- เพิ่ม external EEPROM AT24C32D (I2C1, Addr.0x50) สำหรับเก็บค่า configuration แทน flash ของ MCU
- เซนเซอร์อุณหภูมิ 2 ตัวบน I2C1: STS40-CD1B-R3 (วัดอุณหภูมิห้อง, Addr.0x46) และ STS40-AD1B-R3 (วัดอุณหภูมิบอร์ด, Addr.0x44)
- ช่องต่อ Servo 2 ช่อง (PC6, PC7) — จองไว้สำหรับอนาคต

### Notes
- ตาราง Modbus ออกแบบใหม่ได้อิสระ ไม่ผูกกับชุดคำสั่งของบอร์ดรุ่นก่อนหน้า — ดูคอลัมน์ R5.0 ใน `LGS-Control-Table.md`
- Firmware ของบอร์ด < R5.0 (R4.x / STM32F103) อยู่ใน `assets/` และไม่ใช้ร่วมกับ R5.0

---

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

| Feature | R4.0 | R4.0.1 | R4.1 | R4.2 | R4.3 | R5.0 | R5.1 |
|---------|------|--------|------|------|------|------|------|
| 12V Input Support | ✗ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 8-Color RGB LEDs | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Modbus RTU | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Latch Control | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Function Switch | ✗ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Second Function Switch (SW3/PC13) | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ |
| RUN Status LED | ✗ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Temp Sensor | ✗ | ✗ | ✓ | ✓ | ✓ | ✓ (×2: ห้อง+บอร์ด) | ✓ (×2) |
| Input Current Sense (INA180A4) | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ (ไม่ถูกอ่าน) | ✓ (reg 22, FW ≥ v3.2.0) |
| Surge Protection | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| External EEPROM (AT24C32D) | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Servo Outputs (reserved) | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |

### Current Consumption (Typical @ 12V)

| Mode | Current | Notes |
|------|---------|-------|
| Idle | N/A | Microcontroller + Latch circuit |
| RUN (LEDs off) | N/A | With status LED blinking |
| RUN (1 LED @ 80%) | N/A | Single LED at 80% brightness |
| RUN (All LEDs @ 80%) | N/A | All 8 LEDs at 80% brightness |
| Small Latch Unlock | 300 mA | During solenoid activation @300ms|
| Large Latch Unlock | 2,000 mA | During solenoid activation @500ms|
