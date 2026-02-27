# Arduino Uno Q Terminal Dictionary
**พจนานุกรมคำสั่ง Terminal สำหรับการพัฒนาระบบ Modbus Gateway บนบอร์ด Arduino Uno Q (Debian Linux)**
*อัปเดตล่าสุด: เฟสการติดตั้งและทดสอบระบบ LGS*

---

## 1. System & Package Management (การจัดการระบบและแพ็กเกจ)

| คำสั่ง (Command) | คำอธิบาย (Description) |
| :--- | :--- |
| `sudo apt update` | อัปเดตรายการแพ็กเกจ (package index) ของระบบปฏิบัติการให้ตรงกับ Repository ล่าสุด |
| `sudo apt install python3 python3-pip python3-venv -y` | ติดตั้ง Python 3, ตัวจัดการแพ็กเกจ pip และเครื่องมือสร้าง Virtual Environment โดยข้ามการยืนยัน (`-y`) |
| `pwd` | แสดง Working Directory ปัจจุบัน (Print Working Directory) เช่น `/home/arduino/lgs_gateway` |

---

## 2. Python & Virtual Environment (การจัดการสภาพแวดล้อม Python)

| คำสั่ง (Command) | คำอธิบาย (Description) |
| :--- | :--- |
| `mkdir ~/lgs_gateway` | สร้างไดเรกทอรี `lgs_gateway` ภายใต้ Home directory (`~`) |
| `cd ~/lgs_gateway` | เปลี่ยน Working Directory ไปยังไดเรกทอรี `lgs_gateway` |
| `python3 -m venv venv` | สร้าง Virtual Environment ชื่อ `venv` เพื่อแยกการจัดการไลบรารีของโปรเจกต์ออกจากระบบหลัก |
| `source venv/bin/activate` | **[จำเป็น]** เปิดใช้งาน Virtual Environment — ต้องดำเนินการทุกครั้งก่อนรันสคริปต์หรือติดตั้งไลบรารีเพิ่มเติม |
| `pip install pymodbus pyserial` | ติดตั้งไลบรารี pymodbus (Modbus TCP/RTU) และ pyserial (Serial Communication) |
| `python3 modbus_gateway.py` | รันสคริปต์ Gateway โดยตรง (สำหรับการทดสอบด้วยตนเอง) |

---

## 3. RS485 & Serial Port Management (การจัดการพอร์ตสื่อสาร)

| คำสั่ง (Command) | คำอธิบาย (Description) |
| :--- | :--- |
| `ls /dev/ttyUSB* /dev/ttyACM*` | แสดงรายชื่อพอร์ต USB-to-Serial ที่ระบบตรวจพบ (เช่น `/dev/ttyUSB0`) |
| `sudo dmesg \| grep tty` | ตรวจสอบ Kernel Log เพื่อระบุอุปกรณ์ Serial ที่เชื่อมต่อล่าสุด |
| `python3 -m serial.tools.list_ports` | สแกนและแสดงรายชื่อพอร์ต Serial ทั้งหมดที่ระบบรู้จัก พร้อมรายละเอียดอุปกรณ์ |
| `sudo usermod -a -G dialout $USER` | **[แก้ไข Permission]** เพิ่มผู้ใช้ปัจจุบันเข้ากลุ่ม `dialout` เพื่อให้สามารถอ่าน/เขียนพอร์ต Serial ได้ — ต้อง Logout แล้ว Login ใหม่จึงจะมีผล |

---

## 4. Git & Version Control (การจัดการเวอร์ชันซอร์สโค้ด)

| คำสั่ง (Command) | คำอธิบาย (Description) |
| :--- | :--- |
| `git --version` | ตรวจสอบเวอร์ชัน Git ที่ติดตั้งอยู่ในระบบ |
| `sudo apt install git -y` | ติดตั้ง Git บนบอร์ด |
| `git config --global user.name "..."` | กำหนดชื่อผู้ใช้ Git ระดับ Global สำหรับบันทึก Commit |
| `git config --global user.email "..."` | กำหนดอีเมลผู้ใช้ Git ระดับ Global สำหรับบันทึก Commit |
| `git clone <URL>` | โคลน Repository จาก Remote มายังบอร์ด (เช่น `git clone https://github.com/...`) |

---

## 5. Systemd Service (การกำหนดค่า Background Service)

| คำสั่ง (Command) | คำอธิบาย (Description) |
| :--- | :--- |
| `sudo nano /etc/systemd/system/lgs_gateway.service` | สร้างหรือแก้ไขไฟล์ Unit Configuration ของ Gateway Service |
| `sudo systemctl daemon-reload` | โหลดไฟล์ Unit Configuration ใหม่ — จำเป็นต้องดำเนินการทุกครั้งหลังแก้ไขไฟล์ `.service` |
| `sudo systemctl enable lgs_gateway.service` | กำหนดให้ Service เริ่มทำงานอัตโนมัติเมื่อบอร์ดบูต (Auto-start on boot) |
| `sudo systemctl start lgs_gateway.service` | เริ่มการทำงานของ Service ทันที |
| `sudo systemctl stop lgs_gateway.service` | หยุดการทำงานของ Service |
| `sudo systemctl restart lgs_gateway.service` | รีสตาร์ต Service — ใช้หลังจากแก้ไขไฟล์ `.py` เพื่อให้โค้ดที่อัปเดตมีผล |
| `sudo systemctl status lgs_gateway.service` | ตรวจสอบสถานะของ Service (Active / Inactive / Failed) |

---

## 6. Log Monitoring (การตรวจสอบ Log และวิเคราะห์ข้อผิดพลาด)

| คำสั่ง (Command) | คำอธิบาย (Description) |
| :--- | :--- |
| `sudo journalctl -u lgs_gateway.service -f` | แสดง Log ของ Gateway แบบ Real-time (กด `Ctrl + C` เพื่อหยุด) |
| `sudo journalctl -u lgs_gateway.service -n 50` | แสดง Log ย้อนหลัง 50 บรรทัดล่าสุด |