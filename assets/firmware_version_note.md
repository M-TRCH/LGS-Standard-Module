# Firmware Version Note
**แพลตฟอร์ม:** STM32F103  
**ไฟล์:** firmware_stm32f103_*.bin  

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
