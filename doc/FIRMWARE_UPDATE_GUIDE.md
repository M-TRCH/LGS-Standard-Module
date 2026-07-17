# คู่มืออัพเดท Firmware — LGS Standard Module

**บอร์ด R5.0 (STM32G070CBT6) / firmware ≥ v3.0.0** — บอร์ดรุ่นเก่า (R4.x / STM32F103) ดู[ภาคผนวก](#ภาคผนวก-บอร์ด-r4x-stm32f103)

## ภาพรวม

R5.0 มีช่องทางอัพเดท 2 แบบ:

| ช่องทาง | ใช้เมื่อ | ต้องแกะตู้/ต่อสาย SWD |
|---|---|---|
| **OTA ผ่าน RS485** (ส่วนที่ 2) | งานปกติทุกครั้ง — อัพเดตทุกบอร์ดบนบัสพร้อมกันผ่านสายสื่อสารเดิม | ❌ ไม่ต้อง |
| **ST-Link** (ส่วนที่ 1, 3) | ติดตั้งครั้งแรก / กู้คืนบอร์ดที่ boot ไม่ขึ้น | ✅ ต้อง |

ผัง flash ของ R5.0 (รายละเอียด: `include/flash_layout.h`, `ARCHITECTURE.md`):

```
0x08000000  bootloader   4KB   ติดตั้งครั้งเดียว แทบไม่ต้องแตะอีก
0x08001000  application 62KB   ← ตัวที่ OTA อัพเดต (เพดาน image 61,440 bytes)
0x08010800  staging     62KB   พื้นที่รับ image ใหม่ระหว่าง OTA
```

ค่า config (Modbus ID, สี preset, ฯลฯ) อยู่บน EEPROM ภายนอก (AT24) — **รอดทุกกรณี** ไม่ว่าอัพเดตทางไหน

---

## ส่วนที่ 1: ติดตั้งครั้งแรก (บอร์ดใหม่ / บอร์ดที่มาจาก firmware ≤ v3.0.0)

ทำ**ครั้งเดียวต่อบอร์ด**ผ่าน ST-Link — ติดตั้ง bootloader + application:

```powershell
# 1. bootloader (env LGS_BOOT)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e LGS_BOOT -t upload

# 2. application (env หลัก, ลิงก์ที่ offset 0x1000 อยู่แล้ว)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload
```

ตรวจรับงาน: ไฟ RUN LED กะพริบ + อ่าน Modbus reg 1 ได้เวอร์ชันที่คาด (เช่นด้วย
`tools/test_modbus_rtu.py -p COMx --id <id>`) — จากนั้นการอัพเดตทุกครั้งใช้ OTA ได้เลย

> บอร์ดที่เคยรัน v3.0.0 (app ที่ 0x08000000): ขั้นตอนเดียวกันนี้จะย้าย app ไป slot ใหม่ให้เอง
> ค่า config บน AT24 ไม่หาย

---

## ส่วนที่ 2: อัพเดตผ่าน OTA (งานปกติ)

### 2.1 เตรียม image

1. แก้โค้ดเสร็จแล้ว **bump `FW_VERSION`** ใน `include/version.h` (รูปแบบ `ddmmy`
   เช่น `16076` = 16/07/2026) — ใช้ยืนยันหลังอัพเดตว่าเปลี่ยนจริง
2. Build:
   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
   ```
   ได้ไฟล์ `.pio/build/LGS_STM32G070CBT6/firmware.bin`

> ถ้า image เกินเพดาน 61,440 bytes จะ**พังตั้งแต่ตอน build** (ตั้งใจ — กันส่งของที่ใส่
> staging ไม่ลง) ต้องลดขนาดโค้ดก่อน

⚠️ **ทดสอบ image บนบอร์ดโต๊ะก่อนยิงเข้าไซต์เสมอ** — image ที่ CRC ผ่านแต่โค้ดบั๊กจน
boot ไม่ขึ้น ไม่มี rollback อัตโนมัติ ต้องกู้ด้วย ST-Link

### 2.2 ยิงอัพเดต

```powershell
& "$env:USERPROFILE\miniconda3\python.exe" tools\ota_sender.py -p COM30 --ids 21 -f .pio\build\LGS_STM32G070CBT6\firmware.bin
```

หลายบอร์ด: `--ids 21,22,23` — ข้อมูลส่งเป็น **broadcast ทุกตัวรับพร้อมกัน**
(30 ตัวใช้เวลาเท่า 1 ตัว) ใช้เวลา ~80 วินาที ที่ 9600 baud (~20 วิ ที่ 57600)

สคริปต์ทำครบทุกขั้นและรายงานผลรายตัว:

```
[1/8] probe      เช็คอุปกรณ์ + จดเวอร์ชันเดิม
[2/8] metadata   ประกาศขนาด + CRC32
[3/8] enter      coil 505 — บอร์ดลบ staging, จอ OLED ขึ้น 0%
[4/8] stream     ส่ง chunk 128B (จอวิ่ง 0→99)
[5/8] repair     อ่าน bitmap รายตัว → ยิงซ่อมเฉพาะ chunk ที่หาย
[6/8] finalize   coil 506 — บอร์ดตรวจ CRC32 ทั้ง image เอง
[7/8] apply      coil 507 — รีบูต ให้ bootloader คัดลอกลง app slot (~4 วิ)
[8/8] confirm    "FW 13076 -> 16076 [UPDATED]"
```

### 2.3 Options

| Flag | ใช้ทำอะไร |
|---|---|
| `-y` | ข้าม confirm |
| `-b 57600` | บัสที่ตั้ง baud สูง (ตั้งล่วงหน้าด้วย reg 3 + coil 503) |
| `--status` | อ่านสถานะ OTA รายตัว (idle / receiving / verified / failed + สาเหตุ) |
| `--abort` | ยกเลิก session ทั้งบัส (broadcast coil 508) |
| `--broadcast-apply` | สั่ง apply ทีเดียวทั้งบัส (ดีฟอลต์ = unicast ทีละตัวเฉพาะที่ verified) |
| `--gap`, `--repair-rounds` | จูนจังหวะส่ง / จำนวนรอบซ่อม |

### 2.4 เมื่อมีปัญหา — ออกแบบให้พังแล้วปลอดภัยเสมอ

| เหตุการณ์ | ผลลัพธ์ | ต้องทำอะไร |
|---|---|---|
| สัญญาณรบกวน / chunk หาย | repair round ยิงซ่อมอัตโนมัติ | ไม่ต้องทำอะไร |
| สายหลุด / ปิดโปรแกรมกลางทาง | บอร์ด timeout 30 วิ → กลับสู่ปกติ, app เดิมวิ่งต่อ | ยิงใหม่ตั้งแต่ต้น |
| ไฟดับระหว่างส่ง | staging เสีย, app เดิมไม่ถูกแตะ | ยิงใหม่ตั้งแต่ต้น |
| ไฟดับระหว่าง bootloader คัดลอก | header ยัง valid → เปิดไฟแล้ว**คัดลอกซ้ำเอง** | ไม่ต้องทำอะไร |
| image เสีย / CRC ไม่ตรง | finalize ไม่ผ่าน → ไม่ถูก apply | ตรวจไฟล์แล้วยิงใหม่ |
| บางตัว `failed` ในขั้น verify | ตัวนั้นไม่ถูก apply, ตัวอื่นอัพเดตปกติ | `--status` ดูสาเหตุ แล้วยิงซ้ำเฉพาะตัวนั้น |
| app ใหม่ boot ไม่ขึ้น (บั๊กในโค้ด) | บอร์ดเงียบ | กู้ด้วย ST-Link (ส่วนที่ 3) |

หลังอัพเดตจอ OLED ค้างเลข "99" (progress สุดท้าย) — ดับด้วยการเขียน coil 1010 = 0
หรือปล่อยไว้จน power cycle ถัดไป

---

## ส่วนที่ 3: กู้คืนด้วย ST-Link / STM32CubeProgrammer

ใช้เมื่อบอร์ด boot ไม่ขึ้นหรือคุยผ่านบัสไม่ได้แล้วเท่านั้น

**ทาง PlatformIO (แนะนำ):** ทำเหมือน[ส่วนที่ 1](#ส่วนที่-1-ติดตั้งครั้งแรก-บอร์ดใหม่--บอร์ดที่มาจาก-firmware--v300)
— อัพโหลด app อย่างเดียว (`run -t upload`) พอ ถ้า bootloader ยังอยู่ (ปกติอยู่เสมอ
เพราะไม่มีอะไรเขียนทับ slot นั้น)

**ทาง STM32CubeProgrammer (ใช้ไฟล์ .bin):**

| ไฟล์ | Start Address |
|---|---|
| `firmware.bin` ของ app (จาก `.pio/build/LGS_STM32G070CBT6/`) | **`0x08001000`** ⚠️ ไม่ใช่ 0x08000000 |
| `firmware.bin` ของ bootloader (จาก `.pio/build/LGS_BOOT/`) | `0x08000000` |

เปิด **Verify program** + **Run after program** ตามปกติ

> ⚠️ **อย่าใช้ Full chip erase** ถ้าไม่จำเป็น — มันลบ bootloader ทิ้งด้วย (ต้องลงใหม่ทั้งคู่)
> ค่า config ไม่หายอยู่แล้วเพราะอยู่บน AT24 ภายนอก ไม่ใช่ flash ของ MCU

---

## ส่วนที่ 4: การจัดเก็บไฟล์ release

1. Build แล้วคัดลอก `.pio/build/LGS_STM32G070CBT6/firmware.bin` ไปเก็บใน `assets/`
   ตั้งชื่อตามแบบแผน:
   ```
   firmware_stm32g070_v3.0.0_2026-07-17.bin
   ```
2. บันทึกรายละเอียดรุ่นใน `FIRMWARE_VERSION_NOTE.md` (เวอร์ชัน, วันที่, การเปลี่ยนแปลง,
   breaking changes สำหรับ backend)
3. ไฟล์ release ที่เก็บไว้ใช้ยิง OTA ได้ตรงๆ: `ota_sender.py -f assets/firmware_stm32g070_...bin`

---

## ภาคผนวก: บอร์ด R4.x (STM32F103)

บอร์ดรุ่นเก่าไม่ได้ build จาก source tree นี้ — ใช้ไฟล์สำเร็จใน `assets/firmware_stm32f103_*.bin`
อัพโหลดด้วย STM32CubeProgrammer:

1. **Erasing & Programming** → Browse เลือกไฟล์ `.bin` → Start Address `0x08000000`
2. เปิด **Verify program**, **Full Flash memory checksum**, **Run after program**
3. ครั้งแรกหลังผลิตบอร์ด: เปิด **Full chip erase** ใน Automatic Mode ด้วย;
   ครั้งถัดไป**ไม่ต้องเปิด** (เก็บค่า EEPROM ที่ตั้งไว้ — R4.x เก็บ config ใน flash ของ MCU)
4. **Start automatic mode** → เสียบ ST-Link → รอ 100% → กดปุ่ม Reset บนบอร์ด →
   ไฟ RUN LED กะพริบ = สำเร็จ
