"""
Modbus TCP Client - ควบคุมอุปกรณ์ 80 ID
Device IDs: 11-18, 21-28, 31-38, 41-48, 51-58, 61-68, 71-78, 81-88, 91-98, 101-108
เขียนค่า coils ที่ address 1001-1008 สำหรับแต่ละ Device ID
"""

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException
import time

# ตั้งค่าการเชื่อมต่อ
SERVER_IP = "192.168.0.205"
SERVER_PORT = 502
START_ADDRESS = 1001
NUM_COILS = 8  # addresses 1001-1008

# LUT (Lookup Table) สำหรับเก็บ Device IDs ทั้งหมด 80 ตัว
# รูปแบบ: 11-18, 21-28, 31-38, ..., 101-108
DEVICE_IDS = []
for tens in range(1, 11):  # 1x, 2x, 3x, ..., 10x
    for ones in range(1, 9):  # x1-x8
        device_id = tens * 10 + ones
        DEVICE_IDS.append(device_id)

NUM_DEVICES = len(DEVICE_IDS)  # จำนวนอุปกรณ์ทั้งหมด = 80

def write_coils_to_device(client, unit_id, values):
    """
    เขียนค่าไปยัง multiple coils ของอุปกรณ์
    
    Args:
        client: ModbusTcpClient object
        unit_id: Device ID ของอุปกรณ์ (11-18, 21-28, ..., 101-108)
        values: list ของค่า True/False สำหรับ coils 8 ตัว
    
    Returns:
        bool: True ถ้าสำเร็จ, False ถ้าล้มเหลว
    """
    try:
        # เขียน multiple coils
        result = client.write_coils(
            address=START_ADDRESS,
            values=values,
            device_id=unit_id
        )
        
        if result.isError():
            print(f"❌ Unit ID {unit_id}: เขียนค่าล้มเหลว - {result}")
            return False
        else:
            print(f"✅ Unit ID {unit_id}: เขียนค่าสำเร็จ - Coils {START_ADDRESS}-{START_ADDRESS+NUM_COILS-1} = {values}")
            return True
            
    except ModbusException as e:
        print(f"❌ Unit ID {unit_id}: Modbus Exception - {e}")
        return False
    except Exception as e:
        print(f"❌ Unit ID {unit_id}: Error - {e}")
        return False

def set_all_devices_coils(client, values):
    """
    ตั้งค่า coils สำหรับอุปกรณ์ทั้งหมด 80 ตัว
    
    Args:
        client: ModbusTcpClient object
        values: list ของค่า True/False สำหรับ coils 8 ตัว
    """
    success_count = 0
    fail_count = 0
    
    print(f"\n{'='*60}")
    print(f"เริ่มเขียนค่าไปยังอุปกรณ์ทั้งหมด {NUM_DEVICES} ตัว")
    print(f"ค่าที่จะเขียน: {values}")
    print(f"{'='*60}\n")
    
    for idx, unit_id in enumerate(DEVICE_IDS, start=1):
        if write_coils_to_device(client, unit_id, values):
            success_count += 1
        else:
            fail_count += 1
        
        # หน่วงเวลาเล็กน้อยเพื่อไม่ให้ส่งคำสั่งเร็วเกินไป
        time.sleep(0.05)
        
        # หน่วงเวลา 2 วินาทีระหว่างแต่ละชุด (ทุกๆ 8 อุปกรณ์)
        if idx % 8 == 0 and idx < NUM_DEVICES:
            print(f"   ⏸️  หน่วงเวลา 2 วินาที (เสร็จชุดที่ {idx//8})...")
            time.sleep(2)
    
    print(f"\n{'='*60}")
    print(f"สรุปผลการทำงาน:")
    print(f"  ✅ สำเร็จ: {success_count}/{NUM_DEVICES}")
    print(f"  ❌ ล้มเหลว: {fail_count}/{NUM_DEVICES}")
    print(f"{'='*60}\n")

def set_specific_device_coils(client, unit_id, values):
    """
    ตั้งค่า coils สำหรับอุปกรณ์ตัวเดียว
    
    Args:
        client: ModbusTcpClient object
        unit_id: Unit ID ของอุปกรณ์
        values: list ของค่า True/False สำหรับ coils 8 ตัว
    """
    print(f"\n{'='*60}")
    print(f"เขียนค่าไปยัง Unit ID {unit_id}")
    print(f"ค่าที่จะเขียน: {values}")
    print(f"{'='*60}\n")
    
    write_coils_to_device(client, unit_id, values)

def main():
    """ฟังก์ชันหลัก"""
    
    # สร้าง Modbus TCP Client
    print(f"กำลังเชื่อมต่อไปยัง {SERVER_IP}:{SERVER_PORT}...")
    client = ModbusTcpClient(SERVER_IP, port=SERVER_PORT, timeout=3)
    
    # เชื่อมต่อกับ server
    if not client.connect():
        print(f"❌ ไม่สามารถเชื่อมต่อกับ {SERVER_IP}:{SERVER_PORT}")
        return
    
    print(f"✅ เชื่อมต่อสำเร็จ!\n")
    
    # แสดง Device IDs ที่จะใช้งาน
    print(f"📋 Device IDs ทั้งหมด ({NUM_DEVICES} ตัว):")
    print(f"   {DEVICE_IDS[:10]} ... {DEVICE_IDS[-10:]}")
    print()
    
    try:
        # ===== วนลูปสั่งงานแบบต่อเนื่อง =====
        print("🔄 เริ่มวนลูปสั่งงาน (กด Ctrl+C เพื่อหยุด)\n")
        
        loop_count = 0
        while True:
            loop_count += 1
            print(f"\n{'#'*60}")
            print(f"# รอบที่ {loop_count}")
            print(f"{'#'*60}")
            
            # เซต True ทุก coils สำหรับทุกอุปกรณ์
            print("\n>>> ขั้นตอนที่ 1: ตั้งค่าทุก coils เป็น True")
            all_true = [True] * NUM_COILS
            set_all_devices_coils(client, all_true)
            
            time.sleep(1)  # รอสักครู่
            
            # เซต False ทุก coils สำหรับทุกอุปกรณ์
            print("\n>>> ขั้นตอนที่ 2: ตั้งค่าทุก coils เป็น False")
            all_false = [False] * NUM_COILS
            set_all_devices_coils(client, all_false)
            
            time.sleep(1)  # รอสักครู่ก่อนรอบถัดไป
        
    except KeyboardInterrupt:
        print("\n\n⚠️  หยุดการทำงานโดยผู้ใช้")
        print(f"📊 รวมทำงานไปแล้ว {loop_count} รอบ")
    
    finally:
        # ปิดการเชื่อมต่อ
        client.close()
        print("\n✅ ปิดการเชื่อมต่อเรียบร้อย")

if __name__ == "__main__":
    main()
