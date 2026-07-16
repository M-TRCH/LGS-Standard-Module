# Firmware Version Note
**แพลตฟอร์ม:** STM32F103 (≤ v2.x) / STM32G070 (≥ v3.0.0)
**ไฟล์:** firmware_stm32f103_*.bin

## v3.1.0 / FW 16076 (2026-07-16) — R5.0 เท่านั้น
### New Features
- **OTA ผ่าน RS485 broadcast**: bootloader 4KB + app slot 0x08001000 + staging;
  ส่งด้วย `tools/ota_sender.py` ผ่าน Modbus broadcast FC16 (regs 282–389, coils 505–508);
  ตรวจรายตัว + ยิงซ่อม chunk ที่หาย; ทนไฟดับทุกจุด (E2E + fault-injection ผ่านบนบอร์ดจริง)
- **LED Presets 1–8**: Light 1–8 = preset สีบนวงแหวนเดียว แบบ radio switching;
  config regs 110–184 persist ทั้งหมด (settings schema v2, migrate จาก v1 อัตโนมัติ);
  stats ราย preset 210–281; combo ใหม่ 1031–1038 (Light N + Latch + Display)
- **Display ใช้งานจริง**: coil 1010 + reg 60 แสดงเลขใหญ่ 0–99 บน OLED (เกิน clamp เป็น 99)
- **Latch แบ่ง 2 คำสั่ง**: 1019 Force (ไม่เช็ค sense, 500ms คงที่) / 1020 Safety (sense-aware)

### Breaking Changes (สำหรับ backend / การ deploy)
- **บอร์ดเดิมต้อง flash ผ่าน ST-Link หนึ่งครั้ง** (bootloader + app ที่ offset ใหม่)
  จากนั้นอัปเดตผ่าน OTA ได้ตลอด; app image ต้องไม่เกิน 61,440 bytes
- ตัด legacy importer จาก MCU flash (พื้นที่นั้นเป็น staging แล้ว) — config อยู่บน AT24 เท่านั้น
- Enable Light 2–8 (1002–1008) กลับมาใช้ได้ แต่ความหมายใหม่ = เลือก preset สีบนวงแหวนเดียว
  (เปิดตัวใหม่เคลียร์ coil ตัวเก่าอัตโนมัติ); 190/194 fan-out เขียนทุก preset

---

## v3.0.0 (2026-07-13) — R5.0 เท่านั้น
### Compatibility
- R5.0 (STM32G070CBT6) — ใช้กับบอร์ด R4.x ไม่ได้

### Architecture
- Refactor ยกเครื่องทั้งโครงสร้าง: เลเยอร์ drivers/svc/app, ไม่มี global state ข้าม module,
  ตาราง Modbus แบบ table-driven — ดู `ARCHITECTURE.md`
- Runtime เป็น non-blocking ทั้งหมด: บัส Modbus ตอบสนองระหว่างปลดล็อกกลอนและหน้าต่าง factory reset
- ย้ายที่เก็บค่า R/W(F) จาก flash บน MCU → external EEPROM AT24C32D (I2C1, 0x50)
  พร้อม magic/version/CRC — ค่า config รอดข้ามการ flash firmware เต็มชิป
  (import ค่าเดิมจาก MCU flash ให้อัตโนมัติครั้งแรก)
- ลดขนาด image: 71,016 → 61,404 bytes (เตรียม headroom สำหรับ OTA ผ่าน RS485 ในอนาคต)

### New Features
- เพิ่ม Board Temperature (Addr.21) จากเซนเซอร์ STS40-AD1B; Addr.20 เป็นอุณหภูมิห้องจาก STS40-CD1B
- Baud Rate (Addr.3) มีผลจริง: whitelist 9600/19200/38400/57600, ค่าไม่ถูกต้อง fallback 9600,
  โหมด SET_ID/FACTORY RESET บังคับ 9600 เป็นช่องทางกู้คืน
- Time after unlocking (Addr.40) รายงานค่าจริง (เดิมรายงาน 0 ตลอด)
- จอง Display commands (Addr.60, coils 1010/1011) ไว้ในโค้ด (ยังไม่เรนเดอร์)

### Breaking Changes (สำหรับ backend)
- ชุดคำสั่งเหลือเฉพาะคอลัมน์ R5.0 ใน `LGS-Control-Table.md`:
  ตัด Light 2-8 ทุกกลุ่ม (120-184, 220-281, 1002-1008, 1012-1018, 1022-1028) และ reg 81
- Addr.1 (FW version) รายงานเวอร์ชันของ build จริง (เดิมค้างค่าใน EEPROM); Addr.2 = 500
- Coil 1020/1021 อ่านค่า 1 ได้ระหว่าง pulse (เดิมบัสค้างจนจบ); คำขอระหว่าง cooldown ถูกเคลียร์ทันที
- Factory reset เหลือ reboot เดียว (เดิม 2 ครั้ง)
- Downgrade กลับ v2.x: firmware เก่าอ่านจาก MCU flash จะเห็นค่า ณ ก่อน migrate (ไม่เห็นค่าใน AT24)

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
