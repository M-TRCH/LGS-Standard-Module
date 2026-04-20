# Modbus RTU Command Reference

> เอกสารอ้างอิงคำสั่ง Modbus RTU ที่ **มีการ implement ในเฟิร์มแวร์** สำหรับ LGS Standard Module  
> ปรับปรุงล่าสุด: 20 เมษายน 2026

---

## ข้อกำหนดการสื่อสาร

| Parameter | Value |
|-----------|-------|
| Protocol | Modbus RTU |
| Physical | RS485 Half-Duplex (Auto-direction) |
| Baud Rate | 9600 (default, configurable) |
| Data Format | 8N1 (8 data bits, No parity, 1 stop bit) |
| Default Slave ID | 247 (configurable, range 1-246) |
| Response Delay | 5 ms (กำหนดค่าได้, ชดเชยเวลาสลับทิศทางของ RS485 Hub) |

---

## Supported Function Codes

| FC (Hex) | FC (Dec) | ชื่อ | สถานะ |
|----------|----------|------|--------|
| 0x01 | 1 | Read Coils | รองรับ |
| 0x02 | 2 | Read Discrete Inputs | รองรับ |
| 0x03 | 3 | Read Holding Registers | รองรับ |
| 0x04 | 4 | Read Input Registers | รองรับ |
| 0x05 | 5 | Write Single Coil | รองรับ |
| 0x06 | 6 | Write Single Register | รองรับ |
| 0x0F | 15 | Write Multiple Coils | รองรับ |
| 0x10 | 16 | Write Multiple Registers | รองรับ |
| 0x11 | 17 | Report Slave ID | รองรับ |
| 0x16 | 22 | Mask Write Register | รองรับ |
| 0x17 | 23 | Read/Write Multiple Registers | รองรับ |

---

## Memory Map

| Area | Start Address | จำนวน | Range |
|------|---------------|-------|-------|
| Coils | 0 | 1100 | 0 – 1099 |
| Discrete Inputs | 0 | 1 | 0 |
| Holding Registers | 0 | 400 | 0 – 399 |
| Input Registers | 0 | 1 | 0 |

---

## Holding Registers (FC03 Read / FC06, FC10 Write)

### Device Group (Read-Only, loaded from EEPROM at boot)

| Address | ชื่อ | ค่าเริ่มต้น | หมายเหตุ |
|---------|------|-------------|----------|
| 0 | Device Type | 30 | 10=Standard, 20=Narcotic, 30=Lite |
| 1 | Firmware Version | 20045 | รูปแบบ ddmmy (เช่น 20045 = 20/04/2025) |
| 2 | Hardware Version | 430 | รูปแบบ mnp (m=major, n=minor, p=production) |

### Device Group (Read/Write/Flash)

| Address | ชื่อ | ค่าเริ่มต้น | ช่วงค่า | หมายเหตุ |
|---------|------|-------------|---------|----------|
| 3 | Baud Rate | 9600 | - | มีผลหลัง Save+Reset (Coil 503) |
| 4 | Identifier (Modbus ID) | 247 | 1-246 | มีผลหลัง Save+Reset (Coil 503) |

### Sensor Group (Read-Only, อัปเดตอัตโนมัติใน main loop)

| Address | ชื่อ | หน่วย | หมายเหตุ |
|---------|------|-------|----------|
| 40 | Time After Unlock | วินาที | ระยะเวลานับจากการปลดล็อกครั้งล่าสุด (แสดงค่า 0 ขณะ latch อยู่ในสถานะล็อก) |

### Configuration Group (Read/Write/Flash)

| Address | ชื่อ | ค่าเริ่มต้น | ช่วงค่า | หมายเหตุ |
|---------|------|-------------|---------|----------|
| 80 | Unlock Delay | 0 | 0-65535 | ระยะเวลาหน่วงก่อนปลดล็อก (หน่วย: มิลลิวินาที) |
| **110** | **LED 1 Brightness** | 80 | 0-100 | เปอร์เซ็นต์ |
| 111 | LED 1 Red | 255 | 0-255 | ค่าสีแดง |
| 112 | LED 1 Green | 0 | 0-255 | ค่าสีเขียว |
| 113 | LED 1 Blue | 0 | 0-255 | ค่าสีน้ำเงิน |
| 114 | LED 1 Max On Time | 3600 | 0-65535 | วินาที (0=ไม่จำกัด) |
| **120** | **LED 2 Brightness** | 80 | 0-100 | |
| 121 | LED 2 Red | 0 | 0-255 | |
| 122 | LED 2 Green | 255 | 0-255 | |
| 123 | LED 2 Blue | 0 | 0-255 | |
| 124 | LED 2 Max On Time | 3600 | 0-65535 | |
| **130** | **LED 3 Brightness** | 80 | 0-100 | |
| 131 | LED 3 Red | 0 | 0-255 | |
| 132 | LED 3 Green | 0 | 0-255 | |
| 133 | LED 3 Blue | 255 | 0-255 | |
| 134 | LED 3 Max On Time | 3600 | 0-65535 | |
| **140** | **LED 4 Brightness** | 80 | 0-100 | |
| 141 | LED 4 Red | 255 | 0-255 | |
| 142 | LED 4 Green | 215 | 0-255 | |
| 143 | LED 4 Blue | 0 | 0-255 | |
| 144 | LED 4 Max On Time | 3600 | 0-65535 | |
| **150** | **LED 5 Brightness** | 80 | 0-100 | |
| 151 | LED 5 Red | 0 | 0-255 | |
| 152 | LED 5 Green | 255 | 0-255 | |
| 153 | LED 5 Blue | 255 | 0-255 | |
| 154 | LED 5 Max On Time | 3600 | 0-65535 | |
| **160** | **LED 6 Brightness** | 80 | 0-100 | |
| 161 | LED 6 Red | 255 | 0-255 | |
| 162 | LED 6 Green | 0 | 0-255 | |
| 163 | LED 6 Blue | 255 | 0-255 | |
| 164 | LED 6 Max On Time | 3600 | 0-65535 | |
| **170** | **LED 7 Brightness** | 80 | 0-100 | |
| 171 | LED 7 Red | 255 | 0-255 | |
| 172 | LED 7 Green | 60 | 0-255 | |
| 173 | LED 7 Blue | 0 | 0-255 | |
| 174 | LED 7 Max On Time | 3600 | 0-65535 | |
| **180** | **LED 8 Brightness** | 80 | 0-100 | |
| 181 | LED 8 Red | 255 | 0-255 | |
| 182 | LED 8 Green | 245 | 0-255 | |
| 183 | LED 8 Blue | 120 | 0-255 | |
| 184 | LED 8 Max On Time | 3600 | 0-65535 | |

> **รูปแบบ LED**: ทุก LED ใช้ Base Address + Offset เดียวกัน  
> สูตร: `LED_N_ADDR = 110 + (N-1) * 10` โดย N = 1..8  
> Offset: +0=Brightness, +1=Red, +2=Green, +3=Blue, +4=MaxOnTime

### Global Configuration (Read/Write)

| Address | ชื่อ | ค่าเริ่มต้น | ช่วงค่า | Action |
|---------|------|-------------|---------|--------|
| 190 | Global Brightness | 0 | 0-100 | เมื่อเขียนค่าใหม่ ระบบจะกำหนดค่าความสว่างให้ LED ทุกดวงพร้อมกัน (ทำงานเฉพาะเมื่อค่าเปลี่ยน) |
| 194 | Global Max On Time | 0 | 0-65535 | เมื่อเขียนค่าใหม่ ระบบจะกำหนดค่า Max On Time ให้ LED ทุกดวงพร้อมกัน (ทำงานเฉพาะเมื่อค่าเปลี่ยน) |

### Status Group (Read-Only, อัปเดตอัตโนมัติใน main loop)

| Address | ชื่อ | หน่วย | หมายเหตุ |
|---------|------|-------|----------|
| 200 | Total LED On Count | ครั้ง | ผลรวมจำนวนครั้งการเปิด LED ทั้ง 8 ดวง (จำกัดสูงสุด 65535) |
| 201 | Total LED On Time | วินาที | ผลรวมเวลาการเปิด LED ทั้ง 8 ดวง (จำกัดสูงสุด 65535) |
| 210 | LED 1 On Counter | ครั้ง | จำนวนครั้งที่ LED 1 ถูกเปิด (จำกัดสูงสุด 65535) |
| 211 | LED 1 On Time | วินาที | เวลาเปิดสะสมของ LED 1 (จำกัดสูงสุด 65535) |
| 220 | LED 2 On Counter | ครั้ง | |
| 221 | LED 2 On Time | วินาที | |
| 230 | LED 3 On Counter | ครั้ง | |
| 231 | LED 3 On Time | วินาที | |
| 240 | LED 4 On Counter | ครั้ง | |
| 241 | LED 4 On Time | วินาที | |
| 250 | LED 5 On Counter | ครั้ง | |
| 251 | LED 5 On Time | วินาที | |
| 260 | LED 6 On Counter | ครั้ง | |
| 261 | LED 6 On Time | วินาที | |
| 270 | LED 7 On Counter | ครั้ง | |
| 271 | LED 7 On Time | วินาที | |
| 280 | LED 8 On Counter | ครั้ง | |
| 281 | LED 8 On Time | วินาที | |

> **สูตร Status**: `LED_N_ON_COUNTER = 210 + (N-1) * 10`, `LED_N_ON_TIME = 211 + (N-1) * 10`

---

## Coils (FC01 Read / FC05, FC0F Write)

### Operation Group

| Address | ชื่อ | Action | ผลลัพธ์ |
|---------|------|--------|---------|
| 500 | Factory Reset (Gate) | เขียน 1 | เปิด gate -- ต้องใช้ร่วมกับ Coil 501 หรือ 502 จึงจะมีผล |
| 501 | Factory Reset Except ID | เขียน 1 (ต้องตั้ง Coil 500=1 ร่วมด้วย) | คืนค่าทั้งหมดเป็นค่าเริ่มต้นจากโรงงาน ยกเว้น Modbus ID จากนั้นรีบูต |
| 502 | Factory Reset All Data | เขียน 1 (ต้องตั้ง Coil 500=1 ร่วมด้วย) | คืนค่าทั้งหมดเป็นค่าเริ่มต้นจากโรงงาน รวมถึง Modbus ID จากนั้นรีบูต |
| 503 | Write to EEPROM | เขียน 1 | บันทึก registers ทั้งหมดลง EEPROM พร้อมบันทึก OTA config จากนั้นรีบูต |
| 504 | Software Reset | เขียน 1 | รีบูตอุปกรณ์ทันที (ไม่บันทึกค่า) |
| 505 | OTA Update | เขียน 1 | เข้าสู่โหมดรับเฟิร์มแวร์ผ่าน RS485 (ไม่กลับเข้า main loop, ต้อง power cycle หากล้มเหลว) |

> **ขั้นตอน Factory Reset**: จำเป็นต้องเขียน Coil 500=1 ร่วมกับ Coil 501=1 หรือ 502=1 ภายในคำสั่ง FC0F เดียวกัน (หรือภายในรอบ poll เดียวกัน)

### Control Group - LED Enable

| Address | ชื่อ | Action เขียน 1 | Action เขียน 0 |
|---------|------|----------------|----------------|
| 1001 | LED 1 Enable | เปิด LED 1 ตามค่าสีและความสว่างที่กำหนดใน registers | ดับ LED 1 |
| 1002 | LED 2 Enable | เปิด LED 2 | ดับ LED 2 |
| 1003 | LED 3 Enable | เปิด LED 3 | ดับ LED 3 |
| 1004 | LED 4 Enable | เปิด LED 4 | ดับ LED 4 |
| 1005 | LED 5 Enable | เปิด LED 5 | ดับ LED 5 |
| 1006 | LED 6 Enable | เปิด LED 6 | ดับ LED 6 |
| 1007 | LED 7 Enable | เปิด LED 7 | ดับ LED 7 |
| 1008 | LED 8 Enable | เปิด LED 8 | ดับ LED 8 |

> **การตรวจจับสถานะ**: LED จะเปลี่ยนสถานะเฉพาะเมื่อค่า coil เปลี่ยนจากค่าเดิม (ทำงานแบบ edge-triggered)  
> สูตรคำนวณ address: `MB_COIL_LED_N_ENABLE = 1001 + (N-1)` เมื่อ N = 1..8

### Control Group - FW Version Demo

| Address | ชื่อ | Action |
|---------|------|--------|
| 1000 | FW Version Demo | เขียน 1: แสดงตัวเลขเวอร์ชันเฟิร์มแวร์เป็นรหัสสีบน LED 1-5 โดย LED 6-8 แสดงสีขาวอ่อน (coil รีเซ็ตกลับเป็น 0 โดยอัตโนมัติ) |

> **ตารางรหัสสี**: 0=White, 1=Red, 2=Green, 3=Blue, 4=Yellow, 5=Cyan, 6=Magenta, 7=Orange, 8=Pink, 9=Purple

### Control Group - Latch Trigger

| Address | ชื่อ | Action |
|---------|------|--------|
| 1020 | Latch Trigger | เขียน 1: หน่วงเวลาตาม unlock_delay จากนั้นปลดล็อก latch เป็นเวลา 300 ms (coil รีเซ็ตกลับเป็น 0 โดยอัตโนมัติ) |

### Control Group - LED + Latch (Combined)

| Address | ชื่อ | Action |
|---------|------|--------|
| 1021 | LED 1 + Latch | เขียน 1: เปิด LED 1, หน่วงเวลาตาม unlock_delay, ปลดล็อก 300 ms (coil รีเซ็ตอัตโนมัติ, ตั้ง Coil 1001=1) |
| 1022 | LED 2 + Latch | เขียน 1: เปิด LED 2 พร้อมปลดล็อก (ตั้ง Coil 1002=1) |
| 1023 | LED 3 + Latch | เขียน 1: เปิด LED 3 พร้อมปลดล็อก (ตั้ง Coil 1003=1) |
| 1024 | LED 4 + Latch | เขียน 1: เปิด LED 4 พร้อมปลดล็อก (ตั้ง Coil 1004=1) |
| 1025 | LED 5 + Latch | เขียน 1: เปิด LED 5 พร้อมปลดล็อก (ตั้ง Coil 1005=1) |
| 1026 | LED 6 + Latch | เขียน 1: เปิด LED 6 พร้อมปลดล็อก (ตั้ง Coil 1006=1) |
| 1027 | LED 7 + Latch | เขียน 1: เปิด LED 7 พร้อมปลดล็อก (ตั้ง Coil 1007=1) |
| 1028 | LED 8 + Latch | เขียน 1: เปิด LED 8 พร้อมปลดล็อก (ตั้ง Coil 1008=1) |

> สูตรคำนวณ address: `MB_COIL_LED_N_LATCH = 1021 + (N-1)` เมื่อ N = 1..8

---

## Address ที่ประกาศไว้แต่ยังไม่มี Implementation (Reserved)

| ชนิด | Address | ชื่อ | สถานะ |
|------|---------|------|-------|
| Register | 20 | Built-in Temperature | ยังไม่มีโค้ดอ่านค่าเซนเซอร์ |
| Register | 60 | Set Number of Displays | ยังไม่มีโค้ดจัดการ |
| Register | 290 | Display On Counter | ยังไม่มีโค้ดจัดการ |
| Register | 291 | Display On Time | ยังไม่มีโค้ดจัดการ |
| Coil | 1010 | Display Enable | ยังไม่มีโค้ดจัดการ |
| Coil | 1011-1018 | LED N Display | ยังไม่มีโค้ดจัดการ |

---

## หมายเหตุทางเทคนิค

### การบันทึกค่าถาวร (Flash/EEPROM)
- ค่าที่เขียนลง Holding Register จะเก็บอยู่ใน **RAM เท่านั้น** จนกว่าจะสั่ง **Coil 503 = 1**
- เมื่อเขียน Coil 503 ระบบจะคัดลอกค่า registers ทั้งหมดลง EEPROM จากนั้น **รีบูตอัตโนมัติ**
- รายการค่าที่สามารถบันทึกถาวรได้: Baud Rate, Identifier, LED Configuration (สี, ความสว่าง, Max On Time), Unlock Delay

### ข้อจำกัดด้านเวลา (Timing Constraints)
- Latch มีกลไกป้องกัน: ระยะเวลาปลดล็อกสูงสุด 500 ms และห้ามสั่งปลดล็อกซ้ำภายใน 2000 ms
- Max On Time: เมื่อ LED ทำงานต่อเนื่องจนเกินค่า Max On Time ที่กำหนด ระบบจะดับ LED โดยอัตโนมัติพร้อมรีเซ็ต Coil Enable เป็น 0
- Global Brightness และ Global Max On Time: ทำงานเฉพาะเมื่อค่าที่เขียนแตกต่างจากค่าเดิม (ไม่ทำงานซ้ำเมื่อเขียนค่าเดิม)

### Response Delay
- ค่าเริ่มต้น 5 ms แทรกระหว่างการรับ request และการส่ง reply
- สามารถปรับค่าในโค้ดได้ผ่าน `RTUServer.setResponseDelay(ms)` 
- วัตถุประสงค์: ชดเชยเวลาสลับทิศทางของวงจร auto-direction บน RS485 Hub

### โหมดการทำงาน (Operating Modes)
Modbus poll ทำงานใน **ทุกโหมด** อย่างไรก็ตาม logic ของ LED Control (Coil 1001-1028) และการเก็บสถิติจะทำงานเฉพาะใน **FUNC_SW_RUN** mode เท่านั้น

| Mode | Function Switch | Modbus Poll | LED Control Logic |
|------|----------------|-------------|-------------------|
| RUN | ไม่กดปุ่ม | ทำงานสมบูรณ์ | ทำงาน |
| DEMO | กดค้าง >1 วินาที | ทำงาน (อ่าน/เขียน registers ได้) | ไม่ทำงาน (LED กะพริบอัตโนมัติ) |
| SET_ID | กดค้าง >5 วินาที | ทำงาน (ใช้ ID=246) | ไม่ทำงาน (LED กะพริบสีน้ำเงิน) |
| FACTORY_RESET | กดค้าง >10 วินาที | ไม่ทำงาน (รีเซ็ตก่อนเข้า poll) | ไม่ทำงาน |
| OTA | กดค้าง >11 วินาที | ไม่ทำงาน (ไม่เข้า main loop) | ไม่ทำงาน |
