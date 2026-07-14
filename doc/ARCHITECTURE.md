# Firmware Architecture (R5.0)

โครงสร้าง firmware ของ LGS Standard Module บน STM32G070CBT6 (Arduino framework + PlatformIO)
อัปเดตล่าสุด: 2026-07-13 (refactor ยกเครื่อง v3.0.0)

## เลเยอร์และกติกา dependency

```
L0  constants   include/board.h (pin map) · include/config.h (tunables/defaults)
                include/version.h (identity) — include อะไรใน project ไม่ได้เลย
L1  util        src/util/periodic_timer.h — Arduino.h เท่านั้น
L2  drivers/    1 อุปกรณ์ต่อ 1 module, device object เป็น file-static, export ฟังก์ชันเท่านั้น
                include ได้แค่ L0 + vendor lib — ห้าม include svc/ หรือ app/
                board_io · rs485_port · led_ring · oled · temp_sensor · eeprom_at24 · servo_out
L3  svc/        settings (AT24 blob) · modbus_map.h (address SSOT) · modbus_server (tables)
                include ได้ L0 + drivers ที่ตัวเองใช้ — ห้าม include app/
L4  app/        app (boot + tick pipeline) · modes · led_control · latch_control · ops
                display_control (stub) · servo_control (stub) — include ได้ทุกชั้น
    main.cpp    include แค่ app.h
```

- **ไม่มี `extern` ตัวแปร/object ข้าม module** — state ทุกตัวเป็น file-static หลัง accessor
  (ข้อยกเว้นเดียวที่จงใจ: `RS485Class rs485` ใน rs485_port ที่ modbus_server ใช้ begin)
- **Raw Modbus address อยู่ได้ที่เดียวคือ `svc/modbus_map.h`** (มี `static_assert` ตรึงค่า)
- ตรวจได้ด้วย grep: `grep -rn '#include "svc/\|#include "app/' src/drivers/` ต้องว่าง

## Main loop — tick pipeline (invariant ด้านความปลอดภัย)

```cpp
// app.cpp :: appRun()
modbusServerTick();     // poll + dispatch handler ตามตาราง watch
latchControlTick(now);  // ★ ก่อน mode เสมอ — FSM กลอน + MOSFET-low invariant
ledControlTick(now);    // ★ ก่อน mode เสมอ — max-on-time + สถิติ
<mode dispatch>         // RUN / DEMO / SET_ID / FACTORY_RESET
```

ห้ามให้ mode logic มาก่อนสอง tick แรก — เป็นการการันตีว่าไม่มี mode ไหนทำให้
การตัด MOSFET หรือ max-on-time enforcement อดตาย ทุก timing ใช้
elapsed-subtraction (`now - start >= interval`) เพื่อรอด millis() rollover (~49.7 วัน)

สิ่งที่ blocking โดยตั้งใจ (อย่า "แก้"):
1. `checkFunctionSwitch()` ตอน boot — รันครั้งเดียวก่อน Modbus เริ่ม
2. การเขียน AT24 (ack-poll ~5ms/page) — เกิดเฉพาะ save ที่ตามด้วย reset
3. NeoPixel `show()` (~480µs) และ STS40 read (~10ms) — จำกัดด้วย cadence

## Latch FSM (app/latch_control)

`IDLE → DELAY(reg80, clamp 0-8000ms) → PULSE(≤500ms หรือ sense หลุด) → COOLDOWN → IDLE`

- COOLDOWN = `LATCH_MIN_INTERVAL` นับจาก**จุดเริ่ม** PULSE (ตรงกับ firmware เดิม)
- ทุก tick: `state != PULSE` → บังคับ MOSFET LOW (invariant เชิงโครงสร้าง)
- **Hardware guard (TIM7)**: ตอนเข้า PULSE จะ arm one-shot timer ISR ที่บังคับ MOSFET LOW
  เมื่อครบความกว้าง pulse — เพดาน 500ms ไม่ขึ้นกับ cadence ของ loop (poll() ของ Modbus
  block ได้หลายร้อย ms ระหว่าง flush response ยาวหรือเจอ noise บนบัส)
- คำขอสำเร็จ: coil เคลียร์เมื่อ pulse จบ + sync enable coil เฉพาะเมื่อ LED ยังติดอยู่จริง
  (`ledControlChannelOn()`); คำขอถูกปฏิเสธ (busy): เคลียร์ทันที
- ทุก reset path ต้องผ่าน `opsSystemReset()` ซึ่งบังคับ MOSFET LOW ก่อน `NVIC_SystemReset`
- ⚠️ ใน `appRun` ต้องอ่าน `now = millis()` **หลัง** `modbusServerTick()` เสมอ — handler
  ประทับเวลาด้วย millis() สด ถ้า now เก่ากว่าจะเกิด uint32 underflow แล้ว delay/max-on-time
  หมดอายุทันทีใน tick เดียวกัน

## Settings (svc/settings บน AT24C32D @0x50, I2C1)

Blob ที่ offset 0: `magic 'LGS5' + schemaVersion + payloadSize + payload + CRC16-CCITT`

ลำดับโหลด: อ่านซ้ำ 3 ครั้ง (transient I2C fault ต้องไม่ถูกมองเป็นชิปเปล่า) — อ่านไม่ได้เลย →
defaults ใน RAM **โดยไม่เขียนทับ** + fault blink / blob สมบูรณ์ → ใช้ (ID ถูกตรวจช่วง 1–247) /
magic แปลกปลอม → import จาก MCU flash เดิม (ครั้งเดียว, เก็บ Modbus ID, เสร็จแล้ว
tombstone สำเนาเก่ากันการคืนชีพ) / magic ตรงแต่ CRC พัง → defaults + กู้ identifier /
AT24 ไม่ตอบ → defaults ใน RAM + RUN LED กะพริบถี่ (`STORAGE_FAULT_BLINK_MS`)

การเขียน: save ปกติแตะเฉพาะ payload+CRC (`writePayload`) — ไม่เขียน header/magic ซ้ำ
→ ไฟดับกลาง save ได้อย่างแย่แค่ CRC พัง (เข้าเส้นกู้ identifier) ไม่มีทาง magic หาย;
การ format ครั้งแรกเขียน payload ก่อนแล้วค่อย commit header

เพิ่ม field ใหม่: เพิ่มใน `Settings` + bump `SETTINGS_SCHEMA` + เขียน migration ใน
`settingsInit()` + เพิ่มแถว persist table (ดู cookbook ข้างล่าง)

## Cookbook

**เพิ่มคำสั่ง Modbus ใหม่** (สูตร: const + handler + callback)
1. เพิ่ม address ใน `svc/modbus_map.h` (+`static_assert`) และแถวในตาราง `doc/LGS-Control-Table.md`
2. ถ้า persist ได้ (R/W(F)): เพิ่ม field ใน `Settings` + แถวใน `kPersistRows` (modbus_server.cpp)
3. ลงทะเบียนใน module เจ้าของเรื่อง: `mbRegisterHandler(MB_WATCH_REG_CHANGE | MB_WATCH_COIL_CHANGE | MB_WATCH_COIL_COMMAND, addr, handler)`
   - CHANGE = ยิงครั้งเดียวเมื่อค่าเปลี่ยนจาก shadow (เขียนจากฝั่ง firmware ไม่ยิง)
   - COMMAND = ยิงทุก poll ที่ coil เป็น 1 — handler ต้องเคลียร์ coil เอง

**เพิ่ม operating mode ใหม่**: เพิ่มค่าใน `FunctionSwitchMode` (modes.h) + ช่วงเวลากด
ใน `checkFunctionSwitch()` + `case` ใน appRun + ฟังก์ชัน `runXxxMode()` แบบ non-blocking

**เพิ่ม driver ใหม่**: ไฟล์คู่ใน `src/drivers/` — own object เป็น static, include แค่
board.h + vendor lib, ฟังก์ชัน prefix ชื่อ module

## Flash budget / แผน OTA ผ่าน RS485 broadcast

| จุดวัด | RAM | Flash |
|---|---|---|
| ก่อน refactor (v2.x G070 build) | 5,592 B (15.2%) | 71,016 B (54.2%) |
| หลัง refactor + LTO (v3.0.0) | 5,484 B (14.9%) | **61,404 B (46.8%)** |

- Build flags: `-flto=auto` (ดู platformio.ini) — หลังอัปเดต toolchain ให้ smoke test บนบอร์ดเสมอ
- กติกา: ห้าม String/heap/float ใน runtime path; ตาราง const ใน flash; ไม่มี virtual dispatch
- Config อยู่บน AT24 แล้ว → MCU flash ทั้ง 128KB ใช้กับโค้ดได้เต็ม
- ผัง flash ในอนาคต (ยังไม่ลงมือ — รอ bootloader):
  `[bootloader ~8-16KB] [app slot ≤56KB] [staging slot ≤56KB]`
  คำสั่ง OTA = register/coil ชุดใหม่ผ่าน `mbRegisterHandler` + service เขียน flash แยก module
  ยังไม่ relocate vector table / ไม่แตะ linker script จนกว่า bootloader จะเกิดจริง

## Verification gates

- ทุก commit: `pio run` เขียว + จด flash/RAM เทียบตาราง budget
- Grep gates: layering (ด้านบน) + `grep -rn 'delay(' src/` ต้องเหลือเฉพาะ boot path ใน modes.cpp
- Golden-log regression: sweep อ่าน/เขียนทุก address ใน R5.0 map ด้วยสคริปต์ `tools/`
  เทียบกับ log ที่บันทึกจาก firmware ก่อนหน้า — เป็น merge gate ของการแตะ modbus/latch
- Hardware gates ค้างอยู่ (ต้องบอร์ดจริงก่อน release):
  (a) EEPROM migration: flash ทับเครื่องที่มี config เดิมใน MCU flash → ID/สีต้องรอดไป AT24
  (b) Latch: วัด pulse ≤500ms, sense early-exit, cooldown rejection, Modbus ตอบระหว่าง pulse
  (c) Baud: reg 3=19200 + coil 503 → ต่อใหม่ที่ 19200; ค่าขยะ fallback 9600; SET_ID กู้ที่ 9600
  (d) SET_ID ยัง enumerate ที่ ID 246; เซนเซอร์คู่ (0x44/0x46) probe เจอครบ; AT24-absent boot
