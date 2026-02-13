"""
Modbus TCP Client - ควบคุมอุปกรณ์หลายตู้ (Multi-IP Support)
รองรับตู้หลายขนาด: 80, 68, 60, 40 ช่อง
เขียนค่า coils ที่ address 1001-1008 สำหรับแต่ละ Device ID
"""

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException
import time
import logging
from datetime import datetime
import os

# ตั้งค่าการเชื่อมต่อ
SERVER_PORT = 502
START_ADDRESS = 1001
NUM_COILS = 8  # addresses 1001-1008

# ตั้งค่า Logger
def setup_logger():
    """ตั้งค่า logging system แบบ real-time"""
    # สร้างโฟลเดอร์ logs ถ้ายังไม่มี
    log_dir = "logs"
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    
    # สร้างชื่อไฟล์ log ตาม timestamp
    timestamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    log_filename = os.path.join(log_dir, f"modbus_test_{timestamp}.log")
    
    # ตั้งค่า logging
    logger = logging.getLogger('ModbusTester')
    logger.setLevel(logging.INFO)
    
    # ลบ handlers เก่าออก (ถ้ามี)
    logger.handlers.clear()
    
    # สร้าง file handler (เขียนลงไฟล์)
    file_handler = logging.FileHandler(log_filename, encoding='utf-8')
    file_handler.setLevel(logging.INFO)
    
    # สร้าง console handler (แสดงบน console)
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    
    # สร้าง formatter
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s',
                                  datefmt='%Y-%m-%d %H:%M:%S')
    file_handler.setFormatter(formatter)
    console_handler.setFormatter(formatter)
    
    # เพิ่ม handlers เข้าไปใน logger
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)
    
    return logger, log_filename

# สร้าง logger global
logger, log_file = setup_logger()

# ฟังก์ชันสร้าง Device ID List ตามรูปแบบต่างๆ
def generate_device_ids_80():
    """ตู้ 80 ช่อง: 11-18, 21-28, ..., 101-108"""
    device_ids = []
    for tens in range(1, 11):  # 1x, 2x, ..., 10x
        for ones in range(1, 9):  # x1-x8
            device_ids.append(tens * 10 + ones)
    return device_ids

def generate_device_ids_68():
    """ตู้ 68 ช่อง: 11-18, 21-28, 31-38, 41-48, 51-58, 61-68, 71-78 และ 81-86, 91-96"""
    device_ids = []
    # ชุดแรก: 11-18, 21-28, 31-38, 41-48, 51-58, 61-68, 71-78 (7 ชุด x 8 = 56 ไอดี)
    for tens in range(1, 8):  # 1x-7x
        for ones in range(1, 9):  # x1-x8
            device_ids.append(tens * 10 + ones)
    # ชุดที่สอง: 81-86, 91-96 (2 ชุด x 6 = 12 ไอดี)
    for tens in range(8, 10):  # 8x, 9x
        for ones in range(1, 7):  # x1-x6
            device_ids.append(tens * 10 + ones)
    return device_ids

def generate_device_ids_40():
    """ตู้ 40 ช่อง: 11-14, 21-24, ..., 101-104"""
    device_ids = []
    for tens in range(1, 11):  # 1x, 2x, ..., 10x
        for ones in range(1, 5):  # x1-x4
            device_ids.append(tens * 10 + ones)
    return device_ids

def generate_device_ids_60():
    """ตู้ 60 ช่อง: 11-18, 21-28, 31-38 และ 41-44, 51-54, 61-64, 71-74, 81-84 และ 91-98, 101-108"""
    device_ids = []
    # ชุดแรก: 11-18, 21-28, 31-38 (8 ตัวต่อชุด)
    for tens in range(1, 4):  # 1x, 2x, 3x
        for ones in range(1, 9):  # x1-x8
            device_ids.append(tens * 10 + ones)
    # ชุดกลาง: 41-44, 51-54, 61-64, 71-74, 81-84 (4 ตัวต่อชุด)
    for tens in range(4, 9):  # 4x, 5x, 6x, 7x, 8x
        for ones in range(1, 5):  # x1-x4
            device_ids.append(tens * 10 + ones)
    # ชุดสุดท้าย: 91-98, 101-108 (8 ตัวต่อชุด)
    for tens in range(9, 11):  # 9x, 10x
        for ones in range(1, 9):  # x1-x8
            device_ids.append(tens * 10 + ones)
    return device_ids

# กำหนดค่าตู้ทั้งหมด (Configuration)
CABINETS = [
    {"name": "ตู้ที่ 1",  "ip": "192.168.0.80",  "type": 80},
    {"name": "ตู้ที่ 2",  "ip": "192.168.0.221", "type": 80},
    {"name": "ตู้ที่ 3",  "ip": "192.168.0.76",  "type": 80},
    {"name": "ตู้ที่ 4",  "ip": "192.168.0.210", "type": 80},
    {"name": "ตู้ที่ 5",  "ip": "192.168.0.219", "type": 80},
    {"name": "ตู้ที่ 6",  "ip": "192.168.0.78",  "type": 80},
    {"name": "ตู้ที่ 7",  "ip": "192.168.0.216", "type": 80},
    {"name": "ตู้ที่ 8",  "ip": "192.168.0.214", "type": 80},
    {"name": "ตู้ที่ 9",  "ip": "192.168.0.205", "type": 80},
    {"name": "ตู้ที่ 20", "ip": "192.168.0.201", "type": 68},
]

def get_device_ids_for_cabinet(cabinet_type):
    """ดึง Device IDs ตามประเภทของตู้"""
    if cabinet_type == 80:
        return generate_device_ids_80()
    elif cabinet_type == 68:
        return generate_device_ids_68()
    elif cabinet_type == 60:
        return generate_device_ids_60()
    elif cabinet_type == 40:
        return generate_device_ids_40()
    else:
        raise ValueError(f"ไม่รองรับตู้ประเภท {cabinet_type} ช่อง")

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
            msg = f"❌ Unit ID {unit_id}: เขียนค่าล้มเหลว - {result}"
            print(msg)
            logger.error(f"Unit ID {unit_id}: Write failed - {result}")
            return False
        else:
            msg = f"✅ Unit ID {unit_id}: เขียนค่าสำเร็จ - Coils {START_ADDRESS}-{START_ADDRESS+NUM_COILS-1} = {values}"
            print(msg)
            logger.info(f"Unit ID {unit_id}: Write success - Coils {START_ADDRESS}-{START_ADDRESS+NUM_COILS-1}")
            return True
            
    except ModbusException as e:
        msg = f"❌ Unit ID {unit_id}: Modbus Exception - {e}"
        print(msg)
        logger.error(f"Unit ID {unit_id}: Modbus Exception - {e}")
        return False
    except Exception as e:
        msg = f"❌ Unit ID {unit_id}: Error - {e}"
        print(msg)
        logger.error(f"Unit ID {unit_id}: Unexpected error - {e}")
        return False

def set_all_devices_coils(client, device_ids, values):
    """
    ตั้งค่า coils สำหรับอุปกรณ์ทั้งหมด
    
    Args:
        client: ModbusTcpClient object
        device_ids: list ของ Device IDs
        values: list ของค่า True/False สำหรับ coils 8 ตัว
    
    Returns:
        tuple: (success_count, fail_count)
    """
    num_devices = len(device_ids)
    success_count = 0
    fail_count = 0
    
    print(f"\n{'='*60}")
    print(f"เริ่มเขียนค่าไปยังอุปกรณ์ทั้งหมด {num_devices} ตัว")
    print(f"ค่าที่จะเขียน: {values}")
    print(f"{'='*60}\n")
    
    logger.info(f"Starting to write to {num_devices} devices - Values: {values}")
    
    for idx, unit_id in enumerate(device_ids, start=1):
        if write_coils_to_device(client, unit_id, values):
            success_count += 1
        else:
            fail_count += 1
        
        # หน่วงเวลาเล็กน้อยเพื่อไม่ให้ส่งคำสั่งเร็วเกินไป
        time.sleep(0.05)
        
        # หน่วงเวลา 2 วินาทีระหว่างแต่ละชุด (ทุกๆ 8 อุปกรณ์)
        if idx % 8 == 0 and idx < num_devices:
            print(f"   ⏸️  หน่วงเวลา 2 วินาที (เสร็จชุดที่ {idx//8})...")
            time.sleep(2)
    
    print(f"\n{'='*60}")
    print(f"สรุปผลการทำงาน:")
    print(f"  ✅ สำเร็จ: {success_count}/{num_devices}")
    print(f"  ❌ ล้มเหลว: {fail_count}/{num_devices}")
    print(f"{'='*60}\n")
    
    logger.info(f"Batch write completed - Success: {success_count}/{num_devices}, Failed: {fail_count}/{num_devices}")
    
    return success_count, fail_count

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

def test_cabinet(cabinet, all_true, all_false, cabinet_num, total_cabinets):
    """
    ทดสอบตู้หนึ่งตู้ (สั่ง True ทุก ID แล้วสั่ง False ทุก ID)
    
    Args:
        cabinet: dict ข้อมูลตู้ (name, ip, type)
        all_true: list ค่า True สำหรับ coils
        all_false: list ค่า False สำหรับ coils
        cabinet_num: หมายเลขตู้ปัจจุบัน
        total_cabinets: จำนวนตู้ทั้งหมด
    
    Returns:
        dict: สถิติการทำงานของตู้ (รวมเวลาที่ใช้)
    """
    start_time = time.time()  # เริ่มจับเวลา
    
    print(f"\n{'#'*70}")
    print(f"# [{cabinet_num}/{total_cabinets}] {cabinet['name']} - IP: {cabinet['ip']} - จำนวน {cabinet['type']} ช่อง")
    print(f"{'#'*70}")
    
    logger.info(f"="*70)
    logger.info(f"Starting test - Cabinet [{cabinet_num}/{total_cabinets}]: {cabinet['name']}")
    logger.info(f"IP: {cabinet['ip']}:{SERVER_PORT}, Type: {cabinet['type']} channels")
    
    # สร้าง Device IDs สำหรับตู้นี้
    device_ids = get_device_ids_for_cabinet(cabinet['type'])
    print(f"📋 Device IDs: {device_ids[:5]} ... {device_ids[-5:]} (รวม {len(device_ids)} IDs)\n")
    logger.info(f"Device IDs: {device_ids[:5]} ... {device_ids[-5:]} (Total: {len(device_ids)} IDs)")
    
    # สร้าง Modbus TCP Client
    print(f"กำลังเชื่อมต่อไปยัง {cabinet['ip']}:{SERVER_PORT}...")
    logger.info(f"Attempting connection to {cabinet['ip']}:{SERVER_PORT}")
    client = ModbusTcpClient(cabinet['ip'], port=SERVER_PORT, timeout=3)
    
    # เชื่อมต่อกับ server (ลองสูงสุด 2 ครั้ง)
    connection_attempts = 0
    max_attempts = 2
    connected = False
    
    while connection_attempts < max_attempts and not connected:
        connection_attempts += 1
        
        if client.connect():
            connected = True
            print(f"✅ เชื่อมต่อสำเร็จ!\n")
            logger.info(f"Connection successful on attempt {connection_attempts}/{max_attempts}")
        else:
            if connection_attempts < max_attempts:
                print(f"❌ เชื่อมต่อครั้งที่ {connection_attempts} ล้มเหลว - รอ 20 วินาทีก่อนลองใหม่...")
                logger.warning(f"Connection attempt {connection_attempts} failed - Waiting 20 seconds before retry")
                time.sleep(20)
                print(f"🔄 กำลังลองเชื่อมต่อครั้งที่ {connection_attempts + 1}...")
                logger.info(f"Retrying connection (attempt {connection_attempts + 1}/{max_attempts})")
            else:
                elapsed_time = time.time() - start_time
                print(f"❌ ไม่สามารถเชื่อมต่อกับ {cabinet['ip']}:{SERVER_PORT} หลังจากพยายาม {max_attempts} ครั้ง - ข้าม")
                logger.error(f"Connection failed after {max_attempts} attempts to {cabinet['ip']}:{SERVER_PORT} - Skipping cabinet")
                return {'cabinet': cabinet['name'], 'connected': False, 'success': 0, 'fail': 0, 'elapsed_time': elapsed_time}
    
    if not connected:
        elapsed_time = time.time() - start_time
        logger.error(f"Final connection check failed for {cabinet['name']}")
        return {'cabinet': cabinet['name'], 'connected': False, 'success': 0, 'fail': 0, 'elapsed_time': elapsed_time}
    
    try:
        # เซต True ทุก Device ID เรียงกัน
        print(">>> ขั้นตอนที่ 1: สั่ง TRUE เรียงทุก Device ID")
        logger.info("Step 1: Writing TRUE to all Device IDs")
        success_true, fail_true = set_all_devices_coils(client, device_ids, all_true)
        
        time.sleep(1)  # รอสักครู่
        
        # เซต False ทุก Device ID เรียงกัน
        print(">>> ขั้นตอนที่ 2: สั่ง FALSE เรียงทุก Device ID")
        logger.info("Step 2: Writing FALSE to all Device IDs")
        success_false, fail_false = set_all_devices_coils(client, device_ids, all_false)
        
        elapsed_time = time.time() - start_time
        print(f"⏱️  เวลาที่ใช้: {elapsed_time:.2f} วินาที")
        
        logger.info(f"Cabinet test completed - Total Success: {success_true + success_false}, Total Failed: {fail_true + fail_false}")
        logger.info(f"Elapsed time: {elapsed_time:.2f} seconds")
        logger.info(f"="*70)
        
        return {
            'cabinet': cabinet['name'],
            'connected': True,
            'success': success_true + success_false,
            'fail': fail_true + fail_false,
            'elapsed_time': elapsed_time
        }
    
    finally:
        # ปิดการเชื่อมต่อ
        client.close()
        print(f"🔌 ปิดการเชื่อมต่อ {cabinet['name']}\n")
        logger.info(f"Connection closed for {cabinet['name']}")

def main():
    """ฟังก์ชันหลัก - ทดสอบทุกตู้ (1 Cycle = ทดสอบครบทุกตู้)"""
    
    program_start_time = datetime.now()
    
    print("="*70)
    print(" 🔧 Modbus TCP Multi-Cabinet Tester 🔧")
    print("="*70)
    print(f"\n📊 จำนวนตู้ทั้งหมด: {len(CABINETS)} ตู้")
    print(f"💡 1 Cycle = ทดสอบทุกตู้ (แต่ละตู้สั่ง True และ False ทุก ID)")
    print(f"📝 Log File: {log_file}")
    print(f"\nรายการตู้:")
    for idx, cab in enumerate(CABINETS, 1):
        print(f"   {idx}. {cab['name']:12} - {cab['ip']:15} - {cab['type']:2} ช่อง")
    print()
    
    # เขียน Header ลงใน Log
    logger.info("="*70)
    logger.info("MODBUS TCP MULTI-CABINET TEST - START")
    logger.info("="*70)
    logger.info(f"Start Time: {program_start_time.strftime('%Y-%m-%d %H:%M:%S')}")
    logger.info(f"Total Cabinets: {len(CABINETS)}")
    logger.info("Configuration:")
    logger.info(f"  - Server Port: {SERVER_PORT}")
    logger.info(f"  - Start Address: {START_ADDRESS}")
    logger.info(f"  - Number of Coils: {NUM_COILS}")
    logger.info("")
    logger.info("Cabinet List:")
    for idx, cab in enumerate(CABINETS, 1):
        logger.info(f"  {idx}. {cab['name']:12} - {cab['ip']:15} - {cab['type']:2} channels")
    logger.info("="*70)
    
    # เตรียมค่าสำหรับ coils
    all_true = [True] * NUM_COILS
    all_false = [False] * NUM_COILS
    
    try:
        # ===== วนลูปสั่งงานแบบต่อเนื่อง =====
        print("\n🔄 เริ่มวนลูปสั่งงาน (กด Ctrl+C เพื่อหยุด)\n")
        
        loop_count = 0
        total_stats = {'success': 0, 'fail': 0, 'disconnected': 0}
        
        while True:
            loop_count += 1
            cycle_start_time = time.time()  # เริ่มจับเวลา Cycle
            cycle_start_datetime = datetime.now()
            
            print(f"\n{'='*70}")
            print(f"╔═══ CYCLE #{loop_count} - เริ่มทดสอบทุกตู้ ({len(CABINETS)} ตู้) ═══╗")
            print(f"{'='*70}\n")
            
            logger.info("")
            logger.info("="*70)
            logger.info(f"CYCLE #{loop_count} - START")
            logger.info("="*70)
            logger.info(f"Cycle Start Time: {cycle_start_datetime.strftime('%Y-%m-%d %H:%M:%S')}")
            logger.info(f"Testing {len(CABINETS)} cabinets...")
            
            loop_stats = {'success': 0, 'fail': 0, 'disconnected': 0}
            cabinet_times = []  # เก็บเวลาของแต่ละตู้
            
            # ทดสอบทุกตู้เรียงตามลำดับ
            for idx, cabinet in enumerate(CABINETS, 1):
                result = test_cabinet(cabinet, all_true, all_false, idx, len(CABINETS))
                
                if result['connected']:
                    loop_stats['success'] += result['success']
                    loop_stats['fail'] += result['fail']
                else:
                    loop_stats['disconnected'] += 1
                
                # เก็บข้อมูลเวลาของแต่ละตู้
                cabinet_times.append({
                    'name': result['cabinet'],
                    'time': result['elapsed_time'],
                    'connected': result['connected']
                })
                
                time.sleep(0.5)  # หน่วงเวลาระหว่างตู้
            
            # คำนวณเวลารวมของ Cycle
            cycle_elapsed_time = time.time() - cycle_start_time
            cycle_end_datetime = datetime.now()
            
            # อัปเดตสถิติรวม
            total_stats['success'] += loop_stats['success']
            total_stats['fail'] += loop_stats['fail']
            total_stats['disconnected'] += loop_stats['disconnected']
            
            # แสดงสรุปรอบนี้
            print(f"\n{'='*70}")
            print(f"╚═══ สรุป CYCLE #{loop_count} (ทดสอบครบทั้ง {len(CABINETS)} ตู้แล้ว) ═══╝")
            print(f"   ✅ สำเร็จ: {loop_stats['success']} คำสั่ง")
            print(f"   ❌ ล้มเหลว: {loop_stats['fail']} คำสั่ง")
            print(f"   🔌 ไม่สามารถเชื่อมต่อ: {loop_stats['disconnected']} ตู้")
            print(f"\n   ⏱️  เวลาทดสอบแต่ละตู้:")
            for cab_time in cabinet_times:
                status = "✅" if cab_time['connected'] else "❌"
                print(f"      {status} {cab_time['name']:12} - {cab_time['time']:6.2f} วินาที")
            print(f"\n   🕐 รวมเวลาทั้ง Cycle: {cycle_elapsed_time:.2f} วินาที ({cycle_elapsed_time/60:.2f} นาที)")
            print(f"{'='*70}\n")
            
            # เขียน Summary ลง Log
            logger.info("="*70)
            logger.info(f"CYCLE #{loop_count} - SUMMARY")
            logger.info("="*70)
            logger.info(f"Cycle End Time: {cycle_end_datetime.strftime('%Y-%m-%d %H:%M:%S')}")
            logger.info("Results:")
            logger.info(f"  - Commands SUCCESS: {loop_stats['success']} commands")
            logger.info(f"  - Commands FAILED:  {loop_stats['fail']} commands")
            logger.info(f"  - Cabinets CONNECTED: {len(CABINETS) - loop_stats['disconnected']}/{len(CABINETS)}")
            logger.info(f"  - Cabinets FAILED:    {loop_stats['disconnected']}/{len(CABINETS)}")
            logger.info("")
            logger.info("Cabinet Performance:")
            for cab_time in cabinet_times:
                status = "SUCCESS" if cab_time['connected'] else "FAILED"
                logger.info(f"  [{status}] {cab_time['name']:12} - {cab_time['time']:6.2f} seconds")
            logger.info("")
            logger.info(f"Total Cycle Time: {cycle_elapsed_time:.2f} seconds ({cycle_elapsed_time/60:.2f} minutes)")
            logger.info("="*70)
            
            print(f"⏳ รอ 3 วินาที ก่อนเริ่ม Cycle ถัดไป...\n")
            time.sleep(3)  # รอก่อนรอบถัดไป
        
    except KeyboardInterrupt:
        program_end_time = datetime.now()
        total_runtime = program_end_time - program_start_time
        
        print("\n\n⚠️  หยุดการทำงานโดยผู้ใช้")
        print(f"\n{'='*70}")
        print(f"📊 สรุปการทำงานทั้งหมด ({loop_count} Cycles):")
        print(f"   🔄 จำนวน Cycles ที่เสร็จ: {loop_count}")
        print(f"   ✅ สำเร็จทั้งหมด: {total_stats['success']} คำสั่ง")
        print(f"   ❌ ล้มเหลวทั้งหมด: {total_stats['fail']} คำสั่ง")
        print(f"   🔌 การเชื่อมต่อล้มเหลว: {total_stats['disconnected']} ครั้ง")
        print(f"{'='*70}")
        
        # เขียน Final Summary ลง Log
        logger.info("")
        logger.info("="*70)
        logger.info("PROGRAM TERMINATED - USER INTERRUPT")
        logger.info("="*70)
        logger.info(f"End Time: {program_end_time.strftime('%Y-%m-%d %H:%M:%S')}")
        logger.info(f"Total Runtime: {total_runtime}")
        logger.info("")
        logger.info(f"Overall Statistics ({loop_count} Cycles completed):")
        logger.info(f"  - Total Cycles:              {loop_count}")
        logger.info(f"  - Total Commands SUCCESS:    {total_stats['success']} commands")
        logger.info(f"  - Total Commands FAILED:     {total_stats['fail']} commands")
        logger.info(f"  - Connection Failures:       {total_stats['disconnected']} times")
        logger.info("")
        logger.info("Exit Reason: KeyboardInterrupt (Ctrl+C)")
        logger.info("="*70)
    
    print("\n✅ โปรแกรมสิ้นสุดการทำงาน")
    logger.info("Program ended successfully")

if __name__ == "__main__":
    main()
