"""
Modbus TCP Random Single Coil Tester
Randomly selects cabinet, unit ID, and coil address to test
Each cycle: Write True -> Read Register 40 -> Write False
"""

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException
import time
from datetime import datetime
import os
import csv
import random

# Connection settings
SERVER_PORT = 502
START_ADDRESS = 1001
NUM_COILS = 8  # addresses 1001-1008

# Cabinet configuration
CABINETS = [
    {"name": "Cabinet 1",  "ip": "192.168.0.80",  "type": 80},
    {"name": "Cabinet 2",  "ip": "192.168.0.221", "type": 80},
    {"name": "Cabinet 3",  "ip": "192.168.0.76",  "type": 80},
    {"name": "Cabinet 4",  "ip": "192.168.0.210", "type": 80},
    {"name": "Cabinet 5",  "ip": "192.168.0.219", "type": 80},
    {"name": "Cabinet 6",  "ip": "192.168.0.78",  "type": 80},
    {"name": "Cabinet 7",  "ip": "192.168.0.216", "type": 80},
    {"name": "Cabinet 8",  "ip": "192.168.0.214", "type": 80},
    {"name": "Cabinet 9",  "ip": "192.168.0.205", "type": 80},
    {"name": "Cabinet 20", "ip": "192.168.0.201", "type": 68},
]

# Setup CSV Logger
def setup_csv_logger():
    """Setup CSV logging system"""
    log_dir = "logs"
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    
    timestamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    event_csv_filename = os.path.join(log_dir, f"random_test_{timestamp}.csv")
    
    with open(event_csv_filename, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['Timestamp', 'Cycle', 'Cabinet', 'Unit_ID', 'Address', 
                        'Event_Type', 'Status', 'Value', 'Response_Time_ms', 'Details'])
    
    return event_csv_filename

def log_event_csv(filename, cycle, cabinet, unit_id, address, event_type, status, 
                  value='', response_time='', details=''):
    """Write event to CSV file"""
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    with open(filename, 'a', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, cycle, cabinet, unit_id, address, event_type, 
                        status, value, response_time, details])

# Functions to generate Device ID lists
def generate_device_ids_80():
    """80-channel cabinet: 11-18, 21-28, ..., 101-108"""
    device_ids = []
    for tens in range(1, 11):
        for ones in range(1, 9):
            device_ids.append(tens * 10 + ones)
    return device_ids

def generate_device_ids_68():
    """68-channel cabinet: 11-18, 21-28, 31-38, 41-48, 51-58, 61-68, 71-78 and 81-86, 91-96"""
    device_ids = []
    for tens in range(1, 8):
        for ones in range(1, 9):
            device_ids.append(tens * 10 + ones)
    for tens in range(8, 10):
        for ones in range(1, 7):
            device_ids.append(tens * 10 + ones)
    return device_ids

def get_device_ids_for_cabinet(cabinet_type):
    """Get Device IDs based on cabinet type"""
    if cabinet_type == 80:
        return generate_device_ids_80()
    elif cabinet_type == 68:
        return generate_device_ids_68()
    else:
        raise ValueError(f"Unsupported cabinet type: {cabinet_type} channels")

def write_single_coil(client, unit_id, address, value, cabinet_name, csv_file, cycle):
    """Write single coil value"""
    cmd_start = time.time()
    try:
        result = client.write_coil(
            address=address,
            value=value,
            device_id=unit_id
        )
        response_time = (time.time() - cmd_start) * 1000
        
        if result.isError():
            log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                         'WRITE_SINGLE_COIL', 'FAILED', str(value), 
                         f"{response_time:.2f}", str(result))
            print(f"[FAIL] Unit {unit_id}, Addr {address}: Write {value} failed - {result} | {response_time:.2f}ms")
            return False, response_time
        else:
            log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                         'WRITE_SINGLE_COIL', 'SUCCESS', str(value), 
                         f"{response_time:.2f}", '')
            print(f"[OK] Unit {unit_id}, Addr {address}: Write {value} success | {response_time:.2f}ms")
            return True, response_time
            
    except Exception as e:
        response_time = (time.time() - cmd_start) * 1000
        log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                     'WRITE_SINGLE_COIL', 'ERROR', str(value), 
                     f"{response_time:.2f}", str(e))
        print(f"[ERROR] Unit {unit_id}, Addr {address}: Write error - {e} | {response_time:.2f}ms")
        return False, response_time

def read_single_register(client, unit_id, address, cabinet_name, csv_file, cycle):
    """Read single holding register"""
    cmd_start = time.time()
    try:
        result = client.read_holding_registers(
            address=address,
            count=1,
            device_id=unit_id
        )
        response_time = (time.time() - cmd_start) * 1000
        
        if result.isError():
            log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                         'READ_REGISTER', 'FAILED', '', 
                         f"{response_time:.2f}", str(result))
            print(f"[FAIL] Unit {unit_id}, Reg {address}: Read failed - {result} | {response_time:.2f}ms")
            return False, None, response_time
        else:
            value = result.registers[0]
            log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                         'READ_REGISTER', 'SUCCESS', str(value), 
                         f"{response_time:.2f}", '')
            print(f"[OK] Unit {unit_id}, Reg {address}: Value = {value} | {response_time:.2f}ms")
            return True, value, response_time
            
    except Exception as e:
        response_time = (time.time() - cmd_start) * 1000
        log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                     'READ_REGISTER', 'ERROR', '', 
                     f"{response_time:.2f}", str(e))
        print(f"[ERROR] Unit {unit_id}, Reg {address}: Read error - {e} | {response_time:.2f}ms")
        return False, None, response_time

def run_random_test_cycle(cycle_num, csv_file):
    """Run one random test cycle"""
    print(f"\n{'='*70}")
    print(f"CYCLE #{cycle_num}")
    print(f"{'='*70}")
    
    # 1. Random cabinet
    cabinet = random.choice(CABINETS)
    print(f"Random Cabinet: {cabinet['name']} ({cabinet['ip']})")
    
    # 2. Random unit ID
    device_ids = get_device_ids_for_cabinet(cabinet['type'])
    unit_id = random.choice(device_ids)
    print(f"Random Unit ID: {unit_id}")
    
    # 3. Random coil address (1001-1008)
    coil_address = random.randint(START_ADDRESS, START_ADDRESS + NUM_COILS - 1)
    print(f"Random Coil Address: {coil_address}")
    
    # Log cycle start
    log_event_csv(csv_file, cycle_num, cabinet['name'], unit_id, coil_address,
                 'CYCLE_START', 'INFO', '', '', 
                 f"Cabinet: {cabinet['name']}, Unit: {unit_id}, Addr: {coil_address}")
    
    # Connect to cabinet
    print(f"\nConnecting to {cabinet['ip']}:{SERVER_PORT}...")
    client = ModbusTcpClient(cabinet['ip'], port=SERVER_PORT, timeout=3.0)
    
    if not client.connect():
        print(f"[FAILED] Connection failed!")
        log_event_csv(csv_file, cycle_num, cabinet['name'], '', '',
                     'CONNECTION', 'FAILED', '', '', 'Unable to connect')
        return False
    
    print(f"[CONNECTED] Connection successful!\n")
    log_event_csv(csv_file, cycle_num, cabinet['name'], '', '',
                 'CONNECTION', 'SUCCESS', '', '', '')
    
    try:
        # Step 1: Write single coil = True
        print(f"Step 1: Write coil {coil_address} = True")
        success1, time1 = write_single_coil(client, unit_id, coil_address, True, 
                                            cabinet['name'], csv_file, cycle_num)
        
        time.sleep(3.0)
        
        # Step 2: Read register 40
        print(f"\nStep 2: Read register 40")
        success2, value2, time2 = read_single_register(client, unit_id, 40, 
                                                       cabinet['name'], csv_file, cycle_num)
        
        time.sleep(0.5)
        
        # Step 3: Read register 20
        print(f"\nStep 3: Read register 20")
        success3, value3, time3 = read_single_register(client, unit_id, 20, 
                                                       cabinet['name'], csv_file, cycle_num)
        
        time.sleep(0.5)
        
        # Step 4: Write single coil = False
        print(f"\nStep 4: Write coil {coil_address} = False")
        success4, time4 = write_single_coil(client, unit_id, coil_address, False, 
                                            cabinet['name'], csv_file, cycle_num)
        
        time.sleep(0.5)
        
        # Summary
        total_success = success1 and success2 and success3 and success4
        avg_time = (time1 + time2 + time3 + time4) / 4
        
        print(f"\n{'='*70}")
        print(f"Cycle #{cycle_num} Summary:")
        print(f"  Cabinet: {cabinet['name']}")
        print(f"  Unit ID: {unit_id}")
        print(f"  Coil Address: {coil_address}")
        print(f"  Result: {'SUCCESS' if total_success else 'FAILED'}")
        print(f"  Avg Response: {avg_time:.2f}ms")
        print(f"{'='*70}\n")
        
        log_event_csv(csv_file, cycle_num, cabinet['name'], unit_id, coil_address,
                     'CYCLE_END', 'SUCCESS' if total_success else 'FAILED', '', 
                     f"{avg_time:.2f}", f"All steps completed")
        
        return total_success
        
    finally:
        client.close()
        log_event_csv(csv_file, cycle_num, cabinet['name'], '', '',
                     'CONNECTION', 'CLOSED', '', '', '')

def main():
    """Main function"""
    csv_file = setup_csv_logger()
    
    print("="*70)
    print(" Modbus TCP Random Single Coil Tester")
    print("="*70)
    print(f"\nTotal Cabinets: {len(CABINETS)}")
    print(f"1 Cycle = Random test (Cabinet -> Unit ID -> Coil Address)")
    print(f"Event CSV: {csv_file}")
    print(f"\nAvailable Cabinets:")
    for idx, cab in enumerate(CABINETS, 1):
        print(f"   {idx}. {cab['name']:12} - {cab['ip']:15}")
    print(f"\nUnit ID Range: 11-18, 21-28, ..., 101-108")
    print(f"Coil Address Range: {START_ADDRESS}-{START_ADDRESS + NUM_COILS - 1}")
    print(f"Register Address: 40, 20")
    print(f"\n** Auto-pause: 01:45-02:15 daily (New log file created at 02:15)")
    print()
    
    # Log program start
    log_event_csv(csv_file, 0, 'SYSTEM', '', '', 'PROGRAM_START', 'INFO', '', '', 
                 f"Total Cabinets: {len(CABINETS)}")
    
    try:
        print("\nStarting continuous random testing (Press Ctrl+C to stop)\n")
        
        cycle_count = 0
        success_count = 0
        fail_count = 0
        program_start_time = datetime.now()
        
        while True:
            # Check if current time is in pause window (01:45-02:15)
            current_time = datetime.now().time()
            pause_start = datetime.strptime("01:45", "%H:%M").time()
            pause_end = datetime.strptime("02:15", "%H:%M").time()
            
            if pause_start <= current_time < pause_end:
                # Log pause event
                log_event_csv(csv_file, cycle_count, 'SYSTEM', '', '', 'DAILY_PAUSE', 'INFO', 
                             '', '', f"Pausing at {current_time.strftime('%H:%M:%S')}, will resume at 02:15")
                print(f"\n{'='*70}")
                print(f"[PAUSE] Daily maintenance window (01:45-02:15)")
                print(f"Current time: {current_time.strftime('%H:%M:%S')}")
                print(f"Stopping tests and closing log file...")
                print(f"Will resume at 02:15 with new log file")
                print(f"{'='*70}\n")
                
                # Wait until 02:15
                while datetime.now().time() < pause_end:
                    time.sleep(10)  # Check every 10 seconds
                
                # Create new log file and reset counters for new session
                csv_file = setup_csv_logger()
                success_count = 0
                fail_count = 0
                program_start_time = datetime.now()
                
                print(f"\n{'='*70}")
                print(f"[RESUME] Resuming operations at {datetime.now().time().strftime('%H:%M:%S')}")
                print(f"New log file: {csv_file}")
                print(f"{'='*70}\n")
                
                log_event_csv(csv_file, 0, 'SYSTEM', '', '', 'DAILY_RESUME', 'INFO', 
                             '', '', f"Resumed at {datetime.now().time().strftime('%H:%M:%S')}")
            
            cycle_count += 1
            
            success = run_random_test_cycle(cycle_count, csv_file)
            
            if success:
                success_count += 1
            else:
                fail_count += 1
            
            print(f"Statistics: {success_count} success, {fail_count} failed (Total: {cycle_count} cycles)")
            
            # Delay before next cycle
            print(f"Waiting 2 seconds before next cycle...\n")
            time.sleep(2)
        
    except KeyboardInterrupt:
        program_end_time = datetime.now()
        total_runtime = program_end_time - program_start_time
        
        print("\n\nProgram stopped by user")
        print(f"\n{'='*70}")
        print(f"Overall Summary:")
        print(f"   Total Cycles: {cycle_count}")
        print(f"   Success: {success_count}")
        print(f"   Failed: {fail_count}")
        print(f"   Success Rate: {(success_count/cycle_count*100):.1f}%" if cycle_count > 0 else "N/A")
        print(f"   Runtime: {total_runtime}")
        print(f"{'='*70}")
        
        # Log program end
        log_event_csv(csv_file, cycle_count, 'SYSTEM', '', '', 'PROGRAM_END', 'INFO', 
                     f"Cycles:{cycle_count}", '', 
                     f"Runtime: {total_runtime}, Success: {success_count}, Failed: {fail_count}")
    
    print("\nProgram terminated")
    log_event_csv(csv_file, cycle_count, 'SYSTEM', '', '', 'PROGRAM_TERMINATED', 'INFO', '', '', '')

if __name__ == "__main__":
    main()
