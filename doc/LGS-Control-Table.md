# LGS Control Table

**ที่มา:** ถอดความจาก `LGS-Control-Table-Updated-271125.pdf` (Updated 27/11/25)
**เพิ่มเติม:** คอลัมน์ **R5.0** ระบุคำสั่งที่รองรับบนบอร์ด R5.0 (STM32G070CBT6, firmware ใน repo นี้) — ยืนยันชุด address โดยผู้ดูแลโปรเจกต์ (13/07/26)

## คำอธิบาย

- **Access**: `R` = อ่านอย่างเดียว, `R/W` = อ่าน/เขียน, `R/W(F)` = อ่าน/เขียน และบันทึกลง non-volatile memory ได้ผ่านคำสั่ง Write data to Flash Memory (coil 503)
- **Supported Commands (R4.x)**: ถอดจากช่องสีเขียวใน PDF — R4.0: Bangkok [CBI], R4.0.1: Ban Pong [RBR], R4.3: Nonthavej [BKK] — ⚠️ โปรดตรวจทานกับ PDF ต้นฉบับก่อนอ้างอิงเชิงสัญญา
- **R5.0**: ✓ = มีในโค้ด firmware R5.0, ✗ = ไม่มีในโค้ด (คำสั่งเก่าคงไว้ในเอกสารสำหรับบอร์ด < R5.0 เท่านั้น)
- บอร์ด R5.0 เก็บค่า R/W(F) ลง **external EEPROM AT24C32D** (I2C1, addr 0x50) — ไม่ใช้ flash ของ MCU

## Device (Holding Registers, 2 byte)

| Addr | Data Name | Access | Initial | Range | Unit | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|---|:-:|:-:|:-:|:-:|
| 0 | Device Type | R | - | 10:STANDARD, 20:NARCOTIC ¹ | - | ✓ | ✓ | | ✓ |
| 1 | Firmware Version | R | - | - | - | ✓ | ✓ | | ✓ |
| 2 | Hardware Version | R | - | - | - | ✓ | ✓ | | ✓ ² |
| 3 | Baud Rate | R/W(F) | 9600 | 9600/19200/38400/57600 | bps | | | | ✓ ³ |
| 4 | Modbus Slave ID | R/W(F) | 247 | 1-245, 247 (246:SPECIFIC ห้ามเขียน) | - | ✓ | ✓ | | ✓ ³ |
| 5-6 | Uptime (u32 hi/lo) | R | 0 | - | sec | | | | ✓ ᵈ |
| 7 | Boot Counter | R | 0 | 0-65535 | ครั้ง | | | | ✓ ᵈ |
| 8 | Last Reset Cause | R | - | bitfield | - | | | | ✓ ᵈ |
| 9 | Health Status | R | - | bitfield | - | | | | ✓ ᵈ |
| 10 | Function Mode | R | 0 | 0-3 | - | | | | ✓ ᵈ |
| 11 | Active LED Preset | R | 0 | 0-8 | - | | | | ✓ ᵈ |

¹ firmware ปัจจุบันมีชนิดเพิ่ม: 30:LITE, 40:DELIVERY
² R5.0 รายงานค่า 500; ค่าเวอร์ชันบน R5.0 เป็น compile-time constant (ไม่ค้างใน EEPROM อีกต่อไป)
³ R5.0 **validate ตอน persist (coil 503)**: baud นอก whitelist หรือ ID นอกช่วง (รวม 246 ที่สงวนให้โหมด SET_ID) จะถูกปฏิเสธและ register สะท้อนค่าเดิมกลับ — ไม่มีการ persist ค่าที่ใช้ไม่ได้แล้ว fallback เงียบๆ อีก; โหมด SET_ID/FACTORY RESET ยังบังคับ 9600 เป็นช่องทางกู้คืน
ᵈ กลุ่ม Diagnostics ใหม่ของ R5.0 (อ่านอย่างเดียว, refresh ทุก 1 วินาที): **Uptime** = วินาทีตั้งแต่บูต (จับการรีบูตผิดปกติ) · **Boot Counter** = จำนวนครั้งที่บูต (persist บน AT24, ล้างพร้อมสถิติ) · **Reset Cause** = bit0 IWDG watchdog / bit1 software / bit2 power-on / bit3 NRST pin / bit4 WWDG / bit5 low-power / bit6 option-byte (อ่านแล้วเคลียร์ — แต่ละ boot รายงานสาเหตุของตัวเอง) · **Health** = bit0 AT24 ok / bit1 OLED ok / bit2 room sensor ok / bit3 board sensor ok / bit4 กลอนล็อกอยู่ · **Function Mode** = 0 RUN / 1 DEMO / 2 SET_ID / 3 FACTORY_RESET · **Active Preset** = preset ที่วงแหวนติดอยู่ (0 = ดับ) ไม่ต้องไล่อ่าน coil 1001-1008

## Operation (Coils, 1 bit)

| Addr | Data Name | Access | Initial | Range | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|:-:|:-:|:-:|:-:|
| 500 | Factory Reset | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 501 | Apply Factory Reset (Except ID) | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 502 | Apply Factory Reset (All Data) | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 503 | Write data to Flash Memory ⁴ | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 504 | Software Reset | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 505 | OTA: Enter update mode ᴼ | R/W | FALSE | TRUE/FALSE | | | ✓ | ✓ |
| 506 | OTA: Finalize (verify CRC32) ᴼ | R/W | FALSE | TRUE/FALSE | | | | ✓ |
| 507 | OTA: Apply (reboot + copy) ᴼ | R/W | FALSE | TRUE/FALSE | | | | ✓ |
| 508 | OTA: Abort session ᴼ | R/W | FALSE | TRUE/FALSE | | | | ✓ |
| 509 | Identify (วงแหวนกะพริบขาว ~5s) | R/W | FALSE | TRUE/FALSE | | | | ✓ ᵛ |
| 510 | Clear Statistics | R/W | FALSE | TRUE/FALSE | | | | ✓ ᵛ |
| 511 | All Off (ไฟ + จอ ดับหมด) | R/W | FALSE | TRUE/FALSE | | | | ✓ ᵛ |

ᵛ คำสั่งใหม่ R5.0 (self-clear): **509** = หาตัวเครื่องในแถว — กะพริบขาวชั่วคราวแล้วคืนสีเดิม ไม่กระทบ preset/สถิติ · **510** = ล้างสถิติ (ทั้ง registers และค่า persist) · **511** = คำสั่งเดียวกลับสู่สถานะพัก: วงแหวนดับ จอดับ เคลียร์ coil ตระกูล preset ทุกตัว

ᴼ ดูหัวข้อ "OTA over RS485" ท้ายเอกสาร — coil 505 คง address จากระบบ OTA ของบอร์ดเก่า (R4.3) แต่โปรโตคอลรับส่งเปลี่ยนเป็น Modbus broadcast ทั้งหมด

⁴ บน R5.0 เขียนลง AT24C32D (external EEPROM) — ชื่อคำสั่งคงเดิมเพื่อความเข้ากันได้

## Sensor (Holding Registers, 2 byte)

| Addr | Data Name | Access | Initial | Range | Unit | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|---|:-:|:-:|:-:|:-:|
| 20 | Built-in Temperature ⁵ | R | - | 100-9000 | °C ×100 ⁶ | | | ✓ | ✓ |
| 21 | Board Temperature ⁷ | R | - | 100-9000 | °C ×100 | | | | ✓ |
| 22–23 | *(สำรอง)* | | | | | | | | |
| 40 | Time after unlocking ⁸ | R | - | 0-65535 | sec | | | ✓ | ✓ |
| 41 | Latch Locked Status | R | - | 0/1 | - | | | | ✓ (R5.0 ใหม่: สถานะกลอนตรงๆ 1=ล็อก, ไม่ต้องอนุมานจาก reg 40==0) |
| 41–43 | *(สำรอง)* | | | | | | | | |

⁵ บน R5.0: reg 20 = **อุณหภูมิห้อง** จาก STS40-CD1B-R3 (I2C1 @0x46) — บอร์ดก่อนหน้ามีเซนเซอร์ตัวเดียว
⁶ PDF ต้นฉบับพิมพ์หน่วยว่า ×1000 แต่ช่วง 100-9000 และ firmware ทุกรุ่นส่งค่า °C ×100 (เช่น 25.00°C = 2500)
⁷ ใหม่บน R5.0: **อุณหภูมิบอร์ด** จาก STS40-AD1B-R3 (I2C1 @0x44)
⁸ บอร์ดก่อน R5.0 (firmware ≤ v2.x) ถูกตั้งให้รายงาน 0 ตลอด; R5.0 รายงานค่าจริง (วินาทีนับจากปลดล็อกล่าสุด, เป็น 0 ขณะล็อกอยู่)

## Configuration (Holding Registers, 2 byte)

| Addr | Data Name | Access | Initial | Range | Unit | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|---|:-:|:-:|:-:|:-:|
| 60 | Set the numbers on the display | R/W | 0 | 0-9999 | - | | | | ✓ ⁹ |
| 61–63 | *(สำรอง)* | | | | | | | | |
| 80 | Delay before unlock | R/W(F) | 0 | 0-8000 | ms | | | ✓ | ✓ |
| 81 | ~~LED Num Per Strip~~ ¹⁰ | R/W(F) | 1 | - | - | | | | ✗ |
| 110 | Light 1 Brightness | R/W(F) | 80 | 0-100 | - | ✓ | ✓ | | ✓ ¹¹ |
| 111 | Light 1 Red Value | R/W(F) | 255 | 0-255 | - | ✓ | ✓ | | ✓ |
| 112 | Light 1 Green Value | R/W(F) | 0 | 0-255 | - | ✓ | ✓ | | ✓ |
| 113 | Light 1 Blue Value | R/W(F) | 0 | 0-255 | - | ✓ | ✓ | | ✓ |
| 114 | Light 1 Max On Time Limit | R/W(F) | 3600 | 0-65535 | sec | ✓ | ✓ | | ✓ |
| 120–124 | Light 2 (Brightness/R:0/G:255/B:0/Max On) | R/W(F) | 80/0/255/0/3600 | | | ✓ | ✓ | | ✓ ¹¹ |
| 130–134 | Light 3 (Brightness/R:0/G:0/B:255/Max On) | R/W(F) | 80/0/0/255/3600 | | | ✓ | ✓ | | ✓ |
| 140–144 | Light 4 (Brightness/R:255/G:215/B:0/Max On) | R/W(F) | 80/255/215/0/3600 | | | ✓ | ✓ | | ✓ |
| 150–154 | Light 5 (Brightness/R:0/G:255/B:255/Max On) | R/W(F) | 80/0/255/255/3600 | | | ✓ | ✓ | | ✓ |
| 160–164 | Light 6 (Brightness/R:255/G:0/B:255/Max On) | R/W(F) | 80/255/0/255/3600 | | | ✓ | ✓ | | ✓ |
| 170–174 | Light 7 (Brightness/R:255/G:60/B:0/Max On) | R/W(F) | 80/255/60/0/3600 | | | ✓ | ✓ | | ✓ |
| 180–184 | Light 8 (Brightness/R:255/G:245/B:120/Max On) | R/W(F) | 80/255/245/120/3600 | | | ✓ | ✓ | | ✓ |
| 190 | Global Brightness | R/W | 80 | 0-100 | - | | | ✓ | ✓ ¹² |
| 191–193 | *(สำรอง)* | | | | | | | | |
| 194 | Global Max On Time Limit | R/W | 3600 | 0-65535 | sec | | | ✓ | ✓ ¹² |

⁹ R5.0 เรนเดอร์จริงบน OLED เมื่อ coil 1010 = 1 ด้วยฟอนต์เลขใหญ่ 2 หลัก — **ช่วงที่แสดงคือ 0-99**: ค่าที่เขียนเกิน 99 จะถูก clamp เป็น 99 และรีจิสเตอร์สะท้อนค่าที่ clamp แล้ว; เขียนค่าใหม่ขณะจอเปิดอยู่ → อัปเดตทันที; ค่า volatile (รีเซ็ตเป็น 0 เมื่อรีบูต)
¹⁰ คำสั่งเก่าของบอร์ดรุ่น Delivery (LED 8 เส้นแยกขา) ไม่อยู่ใน Control Table PDF — เลิกใช้และถูกถอดออกจากโค้ด R5.0
¹¹ **แนวคิดใหม่ของ R5.0: Light 1–8 = color preset 1–8 บนวงแหวนเดียวกัน** (ฮาร์ดแวร์มีวงแหวน 16 พิกเซลวงเดียว) — config ราย preset persist ทั้ง 8 ชุด (schema v2 บน AT24; อัปเกรดจาก v1 อัตโนมัติโดยคง config เดิมเป็น preset 1); ค่า default ของ preset 2–8 คือ palette ของ Light 2–8 ในตารางนี้
¹² R5.0: fan-out เขียนลงรีจิสเตอร์ของ**ทุก preset** (110/120/…/180 หรือ 114/124/…/184) ตามความหมาย legacy

## Statistics (Holding Registers, 2 byte)

| Addr | Data Name | Access | Initial | Range | Unit | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|---|:-:|:-:|:-:|:-:|
| 200 | Total Light On Count | R | 0 | 0-65535 | times | | | ✓ | ✓ |
| 201 | Total Light Runtime | R | 0 | 0-65535 | sec | | | ✓ | ✓ |
| 210 | Light 1 On Count | R | 0 | 0-65535 | times | ✓ | ✓ | | ✓ |
| 211 | Light 1 Runtime | R | 0 | 0-65535 | sec | ✓ | ✓ | | ✓ |
| 220–221 | Light 2 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✓ |
| 230–231 | Light 3 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✓ |
| 240–241 | Light 4 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✓ |
| 250–251 | Light 5 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✓ |
| 260–261 | Light 6 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✓ |
| 270–271 | Light 7 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✓ |
| 280–281 | Light 8 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✓ |
| 290 | Display On Count | R | 0 | 0-65535 | times | | | | ✗ |
| 291 | Display Runtime | R | 0 | 0-65535 | sec | | | | ✗ |

## Control (Coils, 1 bit)

| Addr | Data Name | Access | Initial | Range | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|:-:|:-:|:-:|:-:|
| 1001 | Enable Light 1 | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ ¹³ |
| 1002–1008 | Enable Light 2–8 | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ ¹³ |
| 1010 | Enable Display | R/W | FALSE | TRUE/FALSE | | | | ✓ ⁹ |
| 1011 | Enable Light 1 and Display | R/W | FALSE | TRUE/FALSE | | | | ✓ ¹³ |
| 1012–1018 | Enable Light 2–8 and Display | R/W | FALSE | TRUE/FALSE | | | | ✓ ¹³ |
| 1019 | Force Trig Latch | R/W | FALSE | TRUE/FALSE | | | | ✓ ¹⁰ |
| 1020 | Trig Latch (Safety) | R/W | FALSE | TRUE/FALSE | | | ✓ | ✓ ¹⁰ |
| 1021 | Trig Light 1 and Latch | R/W | FALSE | TRUE/FALSE | | | ✓ | ✓ ¹³ |
| 1022–1028 | Trig Light 2–8 and Latch | R/W | FALSE | TRUE/FALSE | | | ✓ | ✓ ¹³ |
| 1031–1038 | Trig Light 1–8 + Latch + Display | R/W | FALSE | TRUE/FALSE | | | | ✓ ¹⁴ |

¹⁰ R5.0 แบ่งการสั่งกลอนเป็น 2 แนวทาง: **1019 Force Trigger** = ยิงเสมอโดยไม่สนใจ sense, pulse คงที่เต็มสเปคสูงสุด 500ms · **1020 Safety Trigger** = sense-aware — ยิงเฉพาะเมื่อ sense อ่านว่ากลอนล็อกอยู่, จ่ายไฟอย่างน้อย 300ms, ต่อไฟขณะยังล็อกจนถึงเพดาน 500ms (คำสั่ง combo ที่มี latch ทุกตัวใช้ลอจิก Safety)
¹³ **Radio switching**: coil ตระกูล Light 1–8 ทั้งหมดขับ**วงแหวนเดียวกัน** โดยเลือก preset สี — เปิด preset ใหม่ขณะตัวเก่าติดอยู่ = วงแหวนเปลี่ยนสีทันทีและ coil ของตัวเก่า (ทั้ง enable และ display-combo) ถูกเคลียร์เป็น 0 อัตโนมัติ; อ่าน coils จะเห็นตัวที่ติดจริงตัวเดียวเสมอ
¹⁴ ใหม่ใน R5.0 (ไม่มีในบอร์ดเก่า): คำสั่งเดียว = เปิด preset N + แสดงเลข reg 60 บนจอ + ยิงกลอน (Safety) — จอติดทันทีที่รับคำสั่ง, coil enable ของ preset ถูก sync หลัง pulse จบ, coil คำสั่ง self-clear เหมือน 1021

## OTA over RS485 (Holding Registers 282–389, R5.0 เท่านั้น)

| Addr | Data Name | Access | ความหมาย |
|---:|---|---|---|
| 282 | OTA State | R | lo byte: 0 idle / 1 receiving / 2 verified / 3 failed · hi byte: error code (1 bad size, 2 bad chunk count, 3 CRC32 mismatch, 4 timeout, 5 flash error, 6 not verified, 7 latch busy, 8 incomplete) |
| 283 | OTA Chunks Received | R | จำนวน chunk ที่รับแล้ว |
| 284–288 | OTA Metadata | W | image size u32 (hi/lo), CRC32 u32 (hi/lo), total chunks — เขียน (broadcast FC16) ก่อนสั่ง coil 505 |
| 290–292 | Chunk Header | W | chunk index, payload length (1–128), payload CRC16-CCITT |
| 293–356 | Chunk Payload | W | 64 registers = 128 bytes (big-endian ต่อ register) |
| 357 | Chunk Commit | W | tx-counter — master เพิ่มค่าทุกการส่ง (รวม retransmit) เพื่อ trigger การประมวลผล chunk |
| 360–389 | Received Bitmap | R | 30 registers = 480 bits (bit ต่อ chunk) — master อ่านรายตัว (unicast FC03) เพื่อหา chunk ที่หายแล้วยิงซ่อม |

Flow: metadata → coil 505 (erase staging ~1s) → stream chunks (broadcast, ทุกตัวบนบัสรับพร้อมกัน) → อ่าน bitmap รายตัว + repair → coil 506 (verify) → coil 507 (apply: เขียน header + รีบูต ให้ bootloader copy) → อ่าน reg 1 ยืนยันเวอร์ชันใหม่. เครื่องมือ: `tools/ota_sender.py`. Image ต้อง build ที่ offset 0x1000 และ ≤ 61,440 bytes. ระหว่างรับ OLED แสดง % ด้วยเลขใหญ่; session ไร้กิจกรรม 30 วินาที = ยกเลิกตัวเอง

## หมายเหตุพฤติกรรม R5.0

- **Validation บน wire (v3.1.x)**: reg 80 เกิน 8000 → clamp เป็น 8000 + สะท้อนทันที; reg 190 เกิน 100 → clamp เป็น 100 + fan-out ค่าที่ clamp; config preset (110-184) ถูก clamp เข้าช่วง (brightness ≤100, RGB ≤255) ตอน persist และ register สะท้อนค่า clamp; **coil 500 ที่เขียนโดยไม่มี 501/502 จะถูกเคลียร์ทิ้ง** (ไม่ค้างเป็นคำสั่งติดอาวุธ)
- **Sensor fault**: reg 20/21 แสดง **0x8000** เมื่ออ่านเซนเซอร์ล้มเหลวติดกัน ≥3 ครั้ง (แทนการค้างค่าสุดท้ายเงียบๆ) — กลับปกติเมื่ออ่านสำเร็จ
- **สถิติ (200-281) persist แล้ว**: เก็บบน AT24 — flush รายชั่วโมง + ก่อน reset ที่สั่งผ่านบัสทุกแบบ (ไฟดับกะทันหันเสียแค่เศษชั่วโมงสุดท้าย); ล้างด้วย coil 510 หรือ factory reset

- **Trig Latch (1019/1020/1021–1028/1031–1038)**: การปลดล็อกเป็นแบบ non-blocking — ระหว่าง pulse บัส Modbus ยังตอบสนอง และ coil อ่านค่า 1 จนกว่า pulse จะเสร็จจึงถูกเคลียร์; คำขอที่ถูกปฏิเสธ (ยังอยู่ในช่วง cooldown 2000ms) coil ถูกเคลียร์ทันที
- **ข้อจำกัดความปลอดภัยกลอน (ใช้กับทั้ง Force และ Safety)**: pulse สูงสุด 500ms (เพดานถูกบังคับซ้ำด้วย hardware timer guard), ระยะห่างระหว่างการปลดล็อกขั้นต่ำ 2000ms (นับจากจุดเริ่ม pulse), delay ก่อนปลดล็อกตาม reg 80
- **Config เก็บเป็น schema v2** บน AT24C32D: บูตแรกหลังอัปเกรดจาก firmware v1 จะ migrate อัตโนมัติ (ค่าเดิมกลายเป็น preset 1, preset 2–8 = palette default)
- **Servo (PC6/PC7)**: ยังไม่มี address — จองไว้สำหรับอนาคต
