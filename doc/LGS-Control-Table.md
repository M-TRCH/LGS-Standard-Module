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
| 3 | Baud Rate | R/W(F) | 9600 | - | bps | | | | ✓ ³ |
| 4 | Modbus Slave ID | R/W(F) | 247 | 1-245, 246:SPECIFIC | - | ✓ | ✓ | | ✓ |

¹ firmware ปัจจุบันมีชนิดเพิ่ม: 30:LITE, 40:DELIVERY
² R5.0 รายงานค่า 500; ค่าเวอร์ชันบน R5.0 เป็น compile-time constant (ไม่ค้างใน EEPROM อีกต่อไป)
³ R5.0 ใช้ค่า baud จริงตอนบูต (whitelist: 9600/19200/38400/57600 — ค่า 115200 เกินช่วง register 16-bit; ค่าไม่ถูกต้อง fallback 9600; โหมด SET_ID/FACTORY RESET บังคับ 9600 เป็นช่องทางกู้คืน) — บอร์ดก่อนหน้า persist ค่าแต่ไม่มีผล

## Operation (Coils, 1 bit)

| Addr | Data Name | Access | Initial | Range | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|:-:|:-:|:-:|:-:|
| 500 | Factory Reset | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 501 | Apply Factory Reset (Except ID) | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 502 | Apply Factory Reset (All Data) | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 503 | Write data to Flash Memory ⁴ | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 504 | Software Reset | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |

⁴ บน R5.0 เขียนลง AT24C32D (external EEPROM) — ชื่อคำสั่งคงเดิมเพื่อความเข้ากันได้

## Sensor (Holding Registers, 2 byte)

| Addr | Data Name | Access | Initial | Range | Unit | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|---|:-:|:-:|:-:|:-:|
| 20 | Built-in Temperature ⁵ | R | - | 100-9000 | °C ×100 ⁶ | | | ✓ | ✓ |
| 21 | Board Temperature ⁷ | R | - | 100-9000 | °C ×100 | | | | ✓ |
| 22–23 | *(สำรอง)* | | | | | | | | |
| 40 | Time after unlocking ⁸ | R | - | 0-65535 | sec | | | ✓ | ✓ |
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
| 110 | Light 1 Brightness | R/W(F) | 80 | 0-100 | - | ✓ | ✓ | | ✓ |
| 111 | Light 1 Red Value | R/W(F) | 255 | 0-255 | - | ✓ | ✓ | | ✓ |
| 112 | Light 1 Green Value | R/W(F) | 0 | 0-255 | - | ✓ | ✓ | | ✓ |
| 113 | Light 1 Blue Value | R/W(F) | 0 | 0-255 | - | ✓ | ✓ | | ✓ |
| 114 | Light 1 Max On Time Limit | R/W(F) | 3600 | 0-65535 | sec | ✓ | ✓ | | ✓ |
| 120–124 | Light 2 (Brightness/R:0/G:255/B:0/Max On) | R/W(F) | 80/0/255/0/3600 | | | ✓ | ✓ | | ✗ ¹¹ |
| 130–134 | Light 3 (Brightness/R:0/G:0/B:255/Max On) | R/W(F) | 80/0/0/255/3600 | | | ✓ | ✓ | | ✗ |
| 140–144 | Light 4 (Brightness/R:255/G:215/B:0/Max On) | R/W(F) | 80/255/215/0/3600 | | | ✓ | ✓ | | ✗ |
| 150–154 | Light 5 (Brightness/R:0/G:255/B:255/Max On) | R/W(F) | 80/0/255/255/3600 | | | ✓ | ✓ | | ✗ |
| 160–164 | Light 6 (Brightness/R:255/G:0/B:255/Max On) | R/W(F) | 80/255/0/255/3600 | | | ✓ | ✓ | | ✗ |
| 170–174 | Light 7 (Brightness/R:255/G:60/B:0/Max On) | R/W(F) | 80/255/60/0/3600 | | | ✓ | ✓ | | ✗ |
| 180–184 | Light 8 (Brightness/R:255/G:245/B:120/Max On) | R/W(F) | 80/255/245/120/3600 | | | ✓ | ✓ | | ✗ |
| 190 | Global Brightness | R/W | 80 | 0-100 | - | | | ✓ | ✓ |
| 191–193 | *(สำรอง)* | | | | | | | | |
| 194 | Global Max On Time Limit | R/W | 3600 | 0-65535 | sec | | | ✓ | ✓ |

⁹ R5.0 จอง address ไว้ (แสดงเลข 0-9999 บน OLED) — การเรนเดอร์จะตามมาใน firmware รุ่นถัดไป
¹⁰ คำสั่งเก่าของบอร์ดรุ่น Delivery (LED 8 เส้นแยกขา) ไม่อยู่ใน Control Table PDF — เลิกใช้และถูกถอดออกจากโค้ด R5.0
¹¹ บอร์ด R5.0 มี LED channel เดียว (Light 1 = วงแหวน 16 พิกเซลทั้งวง) — Light 2–8 คงไว้ในเอกสารสำหรับบอร์ดเก่าเท่านั้น

## Statistics (Holding Registers, 2 byte)

| Addr | Data Name | Access | Initial | Range | Unit | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|---|:-:|:-:|:-:|:-:|
| 200 | Total Light On Count | R | 0 | 0-65535 | times | | | ✓ | ✓ |
| 201 | Total Light Runtime | R | 0 | 0-65535 | sec | | | ✓ | ✓ |
| 210 | Light 1 On Count | R | 0 | 0-65535 | times | ✓ | ✓ | | ✓ |
| 211 | Light 1 Runtime | R | 0 | 0-65535 | sec | ✓ | ✓ | | ✓ |
| 220–221 | Light 2 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✗ |
| 230–231 | Light 3 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✗ |
| 240–241 | Light 4 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✗ |
| 250–251 | Light 5 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✗ |
| 260–261 | Light 6 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✗ |
| 270–271 | Light 7 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✗ |
| 280–281 | Light 8 On Count / Runtime | R | 0 | 0-65535 | | ✓ | ✓ | | ✗ |
| 290 | Display On Count | R | 0 | 0-65535 | times | | | | ✗ |
| 291 | Display Runtime | R | 0 | 0-65535 | sec | | | | ✗ |

## Control (Coils, 1 bit)

| Addr | Data Name | Access | Initial | Range | R4.0 | R4.0.1 | R4.3 | R5.0 |
|---:|---|---|---|---|:-:|:-:|:-:|:-:|
| 1001 | Enable Light 1 | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✓ |
| 1002–1008 | Enable Light 2–8 | R/W | FALSE | TRUE/FALSE | ✓ | ✓ | | ✗ |
| 1010 | Enable Display | R/W | FALSE | TRUE/FALSE | | | | ✓ ⁹ |
| 1011 | Enable Light 1 and Display | R/W | FALSE | TRUE/FALSE | | | | ✓ ⁹ |
| 1012–1018 | Enable Light 2–8 and Display | R/W | FALSE | TRUE/FALSE | | | | ✗ |
| 1019 | Force Trig Latch | R/W | FALSE | TRUE/FALSE | | | | ✓ ¹⁰ |
| 1020 | Trig Latch (Safety) | R/W | FALSE | TRUE/FALSE | | | ✓ | ✓ ¹⁰ |
| 1021 | Trig Light 1 and Latch | R/W | FALSE | TRUE/FALSE | | | ✓ | ✓ |
| 1022–1028 | Trig Light 2–8 and Latch | R/W | FALSE | TRUE/FALSE | | | ✓ | ✗ |

¹⁰ R5.0 แบ่งการสั่งกลอนเป็น 2 แนวทาง: **1019 Force Trigger** = ยิงเสมอโดยไม่สนใจ sense, pulse คงที่เต็มสเปคสูงสุด 500ms · **1020 Safety Trigger** = sense-aware — ยิงเฉพาะเมื่อ sense อ่านว่ากลอนล็อกอยู่, จ่ายไฟอย่างน้อย 300ms, ต่อไฟขณะยังล็อกจนถึงเพดาน 500ms (1021 ใช้ลอจิก Safety เช่นเดียวกับ 1020)

## หมายเหตุพฤติกรรม R5.0

- **Trig Latch (1019/1020/1021)**: การปลดล็อกเป็นแบบ non-blocking — ระหว่าง pulse บัส Modbus ยังตอบสนอง และ coil อ่านค่า 1 จนกว่า pulse จะเสร็จจึงถูกเคลียร์; คำขอที่ถูกปฏิเสธ (ยังอยู่ในช่วง cooldown 2000ms) coil ถูกเคลียร์ทันที
- **ข้อจำกัดความปลอดภัยกลอน (ใช้กับทั้ง Force และ Safety)**: pulse สูงสุด 500ms (เพดานถูกบังคับซ้ำด้วย hardware timer guard), ระยะห่างระหว่างการปลดล็อกขั้นต่ำ 2000ms (นับจากจุดเริ่ม pulse), delay ก่อนปลดล็อกตาม reg 80
- **Servo (PC6/PC7)**: ยังไม่มี address — จองไว้สำหรับอนาคต
