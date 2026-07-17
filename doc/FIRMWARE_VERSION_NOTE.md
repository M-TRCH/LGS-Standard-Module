# Firmware Version Note
**แพลตฟอร์ม:** STM32F103 (≤ v2.x) / STM32G070 (≥ v3.0.0)
**ไฟล์:** firmware_stm32f103_*.bin (R4.x) / firmware_stm32g070_*.bin (R5.0)

## v3.0.0 / FW 17076 (2026-07-17) — release แรกของบอร์ด R5.0

> **build: FW 17076, Device Type 20 (NARCOTIC)** — ส่งขึ้นบอร์ดผ่าน OTA แล้ว
> ไฟล์ release: `assets/firmware_stm32g070_v3.0.0_2026-07-17.bin` (57,884 B, CRC32 435D5D79)
> bootloader: `assets/bootloader_stm32g070_v1.0_2026-07-17.bin` (932 B, ลงครั้งเดียวต่อบอร์ดที่ 0x08000000)

### Compatibility
- R5.0 (STM32G070CBT6) เท่านั้น — ใช้กับบอร์ด R4.x ไม่ได้

### Architecture
- Refactor ยกเครื่องทั้งโครงสร้าง: เลเยอร์ drivers/svc/app, ไม่มี global state ข้าม module,
  ตาราง Modbus แบบ table-driven — ดู `ARCHITECTURE.md`
- Runtime เป็น non-blocking ทั้งหมด: บัส Modbus ตอบสนองระหว่างปลดล็อกกลอนและหน้าต่าง factory reset
- ค่า R/W(F) เก็บบน external EEPROM AT24C32D (I2C1, 0x50) พร้อม magic/version/CRC —
  config รอดข้ามการ flash firmware เต็มชิป
- ผัง flash: bootloader 4KB @0x08000000 + app 62KB @0x08001000 + staging 62KB (สำหรับ OTA)

### New Features
- **OTA ผ่าน RS485 broadcast**: bootloader 4KB + app slot 0x08001000 + staging;
  ส่งด้วย `tools/ota_sender.py` ผ่าน Modbus broadcast FC16 (regs 282–389, coils 505–508);
  ตรวจรายตัว + ยิงซ่อม chunk ที่หาย; ทนไฟดับทุกจุด (E2E + fault-injection ผ่านบนบอร์ดจริง)
- **LED Presets 1–8**: Light 1–8 = preset สีบนวงแหวนเดียว แบบ radio switching;
  config regs 110–184 persist ทั้งหมด (settings schema v2, migrate จาก v1 อัตโนมัติ);
  stats ราย preset 210–281; combo ใหม่ 1031–1038 (Light N + Latch + Display)
- **Display ใช้งานจริง**: coil 1010 + reg 60 แสดงเลขใหญ่ 0–99 บน OLED (เกิน clamp เป็น 99)
- **Latch แบ่ง 2 คำสั่ง**: 1019 Force (ไม่เช็ค sense, 500ms คงที่) / 1020 Safety (sense-aware)
- **Diagnostics (regs 5-11, 41)**: uptime, boot counter, สาเหตุ reset ล่าสุด (จับ watchdog reset),
  health bitfield (AT24/OLED/เซนเซอร์/กลอน), function mode, active preset, สถานะกลอนตรงๆ
- **สถิติ persist บน AT24**: regs 200-281 รอด reboot/OTA (flush รายชั่วโมง + ก่อน reset ที่สั่งเอง);
  coil 510 ล้างสถิติ; เซนเซอร์เสีย ≥3 ครั้งติด → reg 20/21 = 0x8000 (sentinel)
- **คำสั่งใหม่**: coil 509 Identify (กะพริบขาวหาตัวเครื่อง), 511 All Off (ดับไฟ+จอคำสั่งเดียว)
- **Validation แน่นขึ้น**: coil 500 เดี่ยวไม่ค้างเป็นกับดักอีก; baud/ID (รวมห้าม 246) ถูกปฏิเสธ
  ตอน persist พร้อมสะท้อนค่าเดิม; reg 80/190/preset configs clamp+สะท้อนค่าที่ใช้จริง;
  combo 1031-1038 mirror coil 1011-1018 → ปิดครบชุดด้วย 101N=0 คำสั่งเดียว
- เพิ่ม Board Temperature (Addr.21) จากเซนเซอร์ STS40-AD1B; Addr.20 เป็นอุณหภูมิห้องจาก STS40-CD1B
- Baud Rate (Addr.3) มีผลจริง: whitelist 9600/19200/38400/57600, ค่าไม่ถูกต้อง fallback 9600,
  โหมด SET_ID/FACTORY RESET บังคับ 9600 เป็นช่องทางกู้คืน
- Time after unlocking (Addr.40) รายงานค่าจริง (เดิมรายงาน 0 ตลอด)
- SET_ID ตั้ง ID ด้วยปุ่ม Function ได้ (แตะ +1, กดค้างบันทึก) ควบคู่การตั้งผ่าน Modbus ที่ ID 246

### Breaking Changes เทียบกับ v2.x (สำหรับ backend / การ deploy)
- **การติดตั้งครั้งแรกต้อง flash ผ่าน ST-Link หนึ่งครั้ง** (bootloader + app ที่ offset 0x08001000)
  จากนั้นอัปเดตผ่าน OTA ได้ตลอด; app image ต้องไม่เกิน 61,440 bytes
- ชุดคำสั่งตามคอลัมน์ R5.0 ใน `LGS-Control-Table.md` — reg 81 เลิกใช้;
  Light 2–8 (1002–1008 และ config 120–184) **ความหมายใหม่** = เลือก preset สีบนวงแหวนเดียว
  แบบ radio (เปิดตัวใหม่เคลียร์ coil ตัวเก่าอัตโนมัติ); 190/194 fan-out เขียนทุก preset
- Addr.1 (FW version) รายงานเวอร์ชันของ build จริง (เดิมค้างค่าใน EEPROM); Addr.2 = 500
- Coil 1020/1021 อ่านค่า 1 ได้ระหว่าง pulse (เดิมบัสค้างจนจบ); คำขอระหว่าง cooldown ถูกเคลียร์ทันที
- Factory reset เหลือ reboot เดียว (เดิม 2 ครั้ง)
- Config อยู่บน AT24 เท่านั้น (ไม่มี import จาก MCU flash — พื้นที่นั้นเป็น OTA staging แล้ว)

---

## v2.0.2 (2025-12-17)
### Compatibility
- R4.1
- R4.2

### Improvements
- กำหนดเวลาสั่งปลดล็อกสูงสุด ป้องกันกลอนเกิดความร้อนและเสียหาย (Latch Max Unlock = 500ms) 
- กำหนดความถี่การสั่งปลดล็อก ป้องกันกลอนทำงานต่อเนื่องมากเกินไป (Latch Min Interval = 2000ms)

### Bug Fixes
- แก้ไขปัญหาตัวแปรสถิติ overflow (LED On Count, LED On Time)

---

## v2.0.1 (2025-11-27)
### Compatibility
- R4.1
- R4.2

### Bug Fixes
- ปรับเกณฑ์เวลาในการแบ่งโหมดของฟังก์ชัน checkFunctionSwitch:
  - 0-2 วินาที: ไม่มีอะไรเกิดขึ้น (ป้องกันการกดโดยไม่ตั้งใจ)
  - 2-5 วินาที: โหมด Demo (LED กระพริบ 1 ครั้ง/วินาที)
  - 5-8 วินาที: โหมด Set ID (LED กระพริบ 2 ครั้ง/วินาที)
  - 8-11 วินาที: โหมด Factory Reset (LED กระพริบ 4 ครั้ง/วินาที)

---

## v2.0.0 (2025-11-21)
### Compatibility
- R4.1
- R4.2

### New Features
- เพิ่มคำสั่ง Built-in Temperature (Addr. 20) - คืนค่าอุณหภูมิบนบอร์ด
- เพิ่มคำสั่ง Time after unlocking (Addr. 40) - คืนค่าเวลาหลังจากการปลดล็อกกลอนไฟฟ้า
- เพิ่มคำสั่ง Delay before unlock (Addr. 80) - ตั้งค่าการหน่วงเวลาปลดล็อกหลังจากได้รับคำสั่ง
- เพิ่มคำสั่ง LED Latch Control (Addr. 1021-1028) - เปิด LED พร้อมปลดล็อคกลอนในคำสั่งเดียว
- เพิ่มคำสั่ง Global Brightness (Addr. 190) - ตั้งค่าความสว่างทุก LED พร้อมกัน
- เพิ่มคำสั่ง Global Max On Time (Addr. 194) - ตั้งค่า max on-time ทุก LED พร้อมกัน
- เพิ่มการแสดงสถิติรวม Total LED On Count (Addr. 200)
- เพิ่มการแสดงสถิติรวม Total LED On Time (Addr. 201)

### Improvements
- แยกส่วนการทำงาน Enforce และ Statistics ออกจากกันอย่างชัดเจน
- เพิ่มการตรวจสอบเงื่อนไข max on-time = 0 (ไม่จำกัดเวลา)

---

## v1.1.0 (Stable 2025-09-23)
### Compatibility
- R4.0.1 (Ratchaburi-Banpong)

### New Features
- เพิ่มคำสั่งรีเซตผ่าน Modbus
- เพิ่มช่องเก็บข้อมูลสำหรับ Light-on time, count

### Improvements
- ปรับความสว่างเริ่มต้นเป็น 80%
- ปรับจูนค่าสีใหม่ (เหลือง, ส้ม, ขาว) 

---

## v1.0.1 (2025-09-10)
### Compatibility
- R4.0.1

### Improvements
- ปรับความสว่างเริ่มต้นจาก 20% เป็น 90% 
- เพิ่มไฟสถานะ (RUN) กระพริบขณะทำงาน 

### Bug Fixes
- แก้ไขอาการหน่วงตอนเริ่มทำงาน

---

## v1.0.0 (2025-08-28)
### Initial Release
- รองรับการทำงานของ LGS 8 สี
- รองรับการกำหนดไอดีผ่าน Modbus RTU  
