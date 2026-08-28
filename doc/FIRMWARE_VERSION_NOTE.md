# Firmware Version Note
**แพลตฟอร์ม:** STM32F103 (≤ v2.x) / STM32G070 (≥ v3.0.0)
**ไฟล์:** firmware_stm32f103_*.bin (R4.x) / firmware_stm32g070_*.bin (R5.x)

## v3.4.0 / FW 30400 (2026-08-27) — จอเลขใหญ่แสดง 3 หลัก (0-999)

> **OTA ผ่าน RS485**: `.pio/build/LGS_STM32G070CBT6/firmware.bin` (60,092 B, sha256 475fea87…) — **ติดตั้งครบ 64 ตัวบนตู้ทดสอบ (2026-08-27)** ด้วย OTA ทีละแชนแนล ~145 วิ/แชนแนล ไม่มี repair round (หลังแก้ pacing ฝั่ง tool, ดู LGS-Test-Tool `af7a90c`) · จอ 3 หลักผ่านการตรวจด้วยตาบนกระจกจริง · **RELEASED 2026-08-28** (github v3.4.0, factory sha256 17b73e04…)

### Compatibility
- ไม่มีการเปลี่ยน settings/stats schema → OTA จาก v3.3.x ปลอดภัยทั้งบัส
- **สัญญา reg 60 เปลี่ยน**: clamp ขยับจาก 99 → 999 — เอกสารทีมเซิร์ฟเวอร์ที่ประกาศ 0-99 ไว้ต้องแจ้งพร้อมกันตอน deploy (โค้ดฝั่งเซิร์ฟเวอร์ที่เขียนค่า ≤ 99 อยู่แล้วไม่กระทบ)

### Features
- **จอเลขใหญ่รับ 0-999** (`oledPrintLargeNumber` → `uint16_t`): ค่าต่ำกว่า 100 แสดง 2 หลัก
  หน้าตาเดิม ("45" ไม่ใช่ "045") — เกิน 99 จึงงอกหลักที่สาม ขนาดตัวเลขเท่ากันทุกกรณี
- **ฟอนต์ `OledBigNum` ลดจากสูง 54 → 51 px** (`tools/gen_oled_bignum.py --height 51`,
  round-trip check ผ่าน): 3 หลักกว้าง 117/128 px เหลือขอบข้างละ ~5.5 px พ้นเบเซล
  (ฟอนต์เดิมวาด 3 หลักได้แต่เหลือขอบแค่ 2.5 px เสี่ยงโดนเบเซลบัง) และ bitmap เล็กลง ~300 B
- **DEMO counter นับ 0-999** และ seed จาก ID เต็มค่า — บอร์ด ID 108 โชว์ "108" (เดิมถูก % 100 เหลือ "08")

### ขนาด
- Flash 59,888 B (91.4%) — เล็กลง 472 B จาก v3.3.2 แม้โค้ดเพิ่ม เพราะฟอนต์หด · RAM 4,916 B (13.3%)

---

## v3.3.2 / FW 30302 (2026-08-24) — ปิดช่องค้างยาวตอนสายมีสัญญาณรบกวน

> **OTA ผ่าน RS485**: `.pio/build/LGS_STM32G070CBT6/firmware.bin` (60,360 B, sha256 12592dc5…) — ไม่เคยตัด release แยก; ถูกพับรวมเข้า v3.4.0
> ติดตั้งแล้วบนตู้ทดสอบครบ 64 ตัว (2026-08-24, OTA 115 วิ/แชนแนล ไม่มี repair round)

### Compatibility
- ไม่มีการเปลี่ยน settings/stats schema → OTA จาก v3.3.0/v3.3.1 ปลอดภัยทั้งบัส

### Bug Fixes
- **`rs485PortBegin` ตั้ง stream timeout เป็น 2 ms คงที่** (เดิม charMs+20 ≈ 29 ms ที่ 9600)
  `Stream::readBytes` รีสตาร์ท timeout **ต่อไบต์** (`timedRead` เซ็ต `_startMillis` ใหม่ทุกครั้ง)
  ไบต์ขยะหนึ่งตัวที่ถูกตีความเป็น function code ทำให้ libmodbus ขออ่านได้ถึง ~250 ไบต์
  (max-ADU คือเพดานเดียว) สายที่มีสัญญาณรบกวนหยดไบต์ห่างกันน้อยกว่า timeout จึงยืดหนึ่ง
  tick ได้ถึง 250 × 29 ms ≈ 7 วินาที ทะลุ watchdog 4 วินาที
  ประตู frame-gap ใน `modbusServerTick` การันตีว่าเฟรมจริงอยู่ในบัฟเฟอร์ครบก่อน poll()
  การรอไบต์อนาคตจึงไม่มีประโยชน์เลย — กรณีเลวสุดเหลือ 250 × 2 ms = 0.5 วินาที
- **หมายเหตุสำคัญ**: fix นี้ **ไม่ได้** แก้อาการ IWDG ยกแชนแนลที่ตู้ทดสอบเจอ (ติดตั้งแล้วอาการไม่เปลี่ยน
  ดู [[lgs-open-items]] — ต้นเหตุคือการเดินสายยาวร่วมกัน แก้ด้วยการเดินสายใหม่เป็น 8 แชนแนล)
  แต่เป็นการรัดกุมที่ถูกต้องในตัวเอง: สายที่มีปัญหาจะเสียคำตอบ ไม่ทำ CPU ค้าง

---

## v3.3.1 / FW 30301 (2026-08-18) — สถิติสองช่อง กันไฟดับคาการเขียน

> **OTA ผ่าน RS485** ติดตั้งครบ 64 ตัวบนตู้ทดสอบ (2026-08-18) — ไม่ได้ตัด release

### Compatibility
- **layout ของ blob ไม่เปลี่ยน** — `seq` ใช้ที่ของ `reserved2` (offset 86) ซึ่ง v3.3.0 เขียน 0 ไว้อยู่แล้ว
  blob เดิมในสนามจึงอ่านได้เป็น seq 0 ไม่ต้อง migrate ไม่ต้องขยับขนาด ไม่เปลี่ยนจำนวน page write
- OTA จาก v3.3.0 ปลอดภัยทั้งบัส ค่าสถิติสะสมอยู่ครบ

### Bug Fixes
- **เขียนสถิติสลับสองช่องบน AT24** (`STATS_AT24_ADDR` 128 และ `STATS_AT24_ADDR_B` 384)
  เดิมมีช่องเดียวและถูกเขียนทับทุกครั้งที่ตัวนับเปลี่ยน ไฟดับคาการเขียนจึงไม่เหลือ blob ที่ผ่าน CRC
  แล้ว `statsInit()` จะเริ่มทุกตัวนับใหม่จากศูนย์ — ประวัติทั้งชีวิตของโมดูลหายทั้งก้อน
  (โมดูล 97 คือหลักฐานว่าเกิดขึ้นจริง: 72 boots ขณะเพื่อนร่วมแถวอยู่ที่ ~380)
  `statsInit` อ่านทั้งสองช่องแล้วเลือกช่องที่ valid และ `seq` ใหม่กว่า (`seqNewer()` เทียบแบบ signed รองรับ wrap)
  `statsPersistIfChanged` เขียนช่องที่**ไม่ใช่**ช่องที่ใช้อยู่ แล้วค่อยสลับหลัง `at24Write` คืน true
  การเขียนที่ขาดกลางคันจึงทำลายได้แค่สำเนาที่ไม่มีใครอ่าน

---

## v3.3.0 / FW 30300 (2026-08-13) — สถิติ v2, framing, หน้าปัดหน้าเครื่อง

> ติดตั้งครั้งแรกผ่าน ST-Link: `assets/firmware_stm32g070_v3.3.0_factory_2026-08-13.bin` (63,936 B)
> **OTA ผ่าน RS485**: `assets/firmware_stm32g070_v3.3.0_2026-08-13.bin` (59,840 B)
> RELEASED 2026-08-14 — ติดตั้งครบ 64 ตัวบนตู้ทดสอบ

### Compatibility
- **stats blob เปลี่ยนเป็น v2 (92 B)** — อ่าน blob v1 เข้ามาให้ ตัวนับที่สะสมไว้ในสนามจึงไม่หายตอนอัปเกรด
- กลุ่ม register เดิม 200-281 ยังอยู่ครบและยัง clamp เหมือนเดิม เพื่อ master รุ่นเก่า

### Improvements
- **Statistics v2 (reg 400-451)** — ค่า u32 จริง hi word ก่อน: จำนวนครั้ง/เวลาที่ไฟติดรายพรีเซ็ต,
  **จำนวนครั้งที่กลอนทำงาน, จำนวนครั้งที่กดปุ่มยืนยัน, วินาทีที่ทำงานสะสม, จำนวน IWDG reset**
- **หน้าปัดหน้าเครื่อง** — SET_ID ตั้งได้ถึง 108 กดค้างเพื่อไล่เลข บันทึกเองเมื่อปล่อยไว้ครบเวลา,
  ตัวเลขขยายเต็มกรอบ, มีหน้าจอ OTA ของตัวเอง
- **device type ตอน commissioning** + LED-8 mask สำหรับ type 10, ADC แบบ bare-metal

### Bug Fixes
- **RTU-silence framing ใน `modbusServerTick`** — รอความเงียบก่อนส่งบัฟเฟอร์ให้ไลบรารี
  เดิม `readBytes` เป็น blocking และฮับตัดเฟรมครึ่งทางทุกครั้งที่สลับแชนแนล โมดูลบนแชนแนลที่ถูกตัด
  จึงค้างใน poll() จนโดน watchdog — วัดได้ ~0.6 reset ต่อโมดูลต่อรอบที่มีการข้ามแชนแนล
  มองไม่เห็นจากฝั่ง master แต่ช่องที่สว่างอยู่ดับหมด (ดู v3.3.2 สำหรับช่องโหว่ที่ fix นี้ยังเหลือไว้)

---

## v3.2.0 / FW 30200 (2026-08-06) — รองรับบอร์ด R5.1

> ติดตั้งครั้งแรกผ่าน ST-Link (flash 0x08000000): `assets/firmware_stm32g070_v3.2.0_factory_2026-08-06.bin` (65,136 B, sha256 7972a50a…c2bb4d)
> **OTA ผ่าน RS485**: `assets/firmware_stm32g070_v3.2.0_2026-08-06.bin` (61,040 B, sha256 d0a27512…4b38) — app ล้วน ลิงก์ที่ 0x1000
> เพดาน linker 61,440 / slot จริง 63,488

### Compatibility
- **image เดียวรันทั้ง R5.0 และ R5.1** — ปุ่ม SW3 (PC13) บน R5.0 ไม่เดินสาย firmware เปิด internal pull-up ให้เงียบสนิท
- ไม่มีการเปลี่ยน settings schema → **OTA จาก v3.0.0/v3.1.0 ปลอดภัยทั้งบัส** ค่า ID/preset/baud เดิมอยู่ครบ
- reg 2 (Hardware Version) เปลี่ยนจาก 500 → **510 บนทุกบอร์ด** (R5.0 ไม่เข้าสายการผลิตจริง)

### New Features
- **reg 18/19 วงจรยืนยันการหยิบยา** — คนจัดยากดปุ่มหน้าช่อง (KEY1/SW3) = "หยิบแล้ว" · reg 18 นับการกด (master ดูตัวนับเปลี่ยน — ทนต่อ poll ช้า/retry), reg 19 สถานะสด · โมดูลกะพริบแหวนรับทราบ ~0.6s **เป็นสีของ preset ที่ติดอยู่** (identify ยังกะพริบขาว — คนละความหมาย) แล้วคืนสี preset — ไฟดับเมื่อ master สั่งเท่านั้น · เฉพาะโหมด RUN
- **SW3/PC13 mirror switch** — ปุ่มที่สองทำงานแทน KEY1 ได้ทุกจังหวะ (boot mode select / DEMO / SET_ID) สำหรับตู้ที่หน้ากากบังปุ่มใหญ่
- **reg 22 Input Current (mA)** — กระแสขาเข้า 12V ผ่าน INA180A4 + shunt 3 mΩ (PA6), refresh 1 วินาทีในโหมด RUN · วงจรมีตั้งแต่ R5.0 แต่เพิ่งเริ่มอ่าน
- **reg 12–17 Device UID** — UID 96 บิตของชิปเป็น 6 registers, ต่อ hex แล้วตรงกับ `device_uid` ใน commission_log.csv — ยืนยันตัวบอร์ดผ่านบัสได้โดยไม่ต้องเปิดตู้

## v3.1.0 / FW 30100 (2026-08-04) — commissioning ผ่าน ST-Link ครั้งเดียวจบ

> ติดตั้งครั้งแรกผ่าน ST-Link: `assets/firmware_stm32g070_v3.1.0_factory_2026-08-04.bin` (62,380 B, sha256 e6aebd54…9f4579)
> **OTA ผ่าน RS485**: `assets/firmware_stm32g070_v3.1.0_2026-08-04.bin` (58,284 B, sha256 aa10c47c…5a79)

- **commissioning block** ในภาพเฟิร์มแวร์: LGS Test Tool ปะ Modbus ID + token ลงไฟล์ก่อน flash ผ่าน ST-Link — บอร์ดตอบที่ ID นั้นตั้งแต่บูตแรกโดยไม่ต้องต่อ RS485 · apply-once ด้วย token บน AT24 (offset 256) · ไม่ติ๊ก overwrite จะไม่มีวันเปลี่ยน ID บอร์ดที่ตั้งค่าแล้ว
- **reg 1 เปลี่ยนเป็น semantic version** (`major×10000+minor×100+patch`, 30100 = v3.1.0) แทนรหัสวันที่ ddmmy ที่เทียบมากน้อยไม่ได้
- เพิ่ม `tools/post_build_check.py` (gate ตรวจ block ทุก build) และ `tools/make_factory_image.py` (ประกอบ boot+app)

## v3.0.0 / FW 17076 (2026-07-17) — release แรกของบอร์ด R5.0

> **build: FW 17076, Device Type 20 (NARCOTIC)** — ส่งขึ้นบอร์ดผ่าน OTA แล้ว
> ไฟล์ release: `assets/firmware_stm32g070_v3.0.0_2026-07-17.bin` (57,884 B, CRC32 435D5D79)
> bootloader: `assets/bootloader_stm32g070_v1.0_2026-07-17.bin` (932 B, ลงครั้งเดียวต่อบอร์ดที่ 0x08000000)
> factory image (boot+app ไฟล์เดียว flash ที่ 0x08000000): `assets/firmware_stm32g070_v3.0.0_factory_2026-07-17.bin` (61,980 B, CRC32 B7FC6056)

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
