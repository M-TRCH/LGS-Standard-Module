"""
Modbus TCP Client - Multi-Cabinet Control (Multi-IP Support)
Supports multiple cabinet sizes: 80, 68, 60, 40 channels
Writes coil values to addresses 1001-1008 for each Device ID
Reads temperature from holding register address 20
"""

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException
import time
from datetime import datetime
import os
import csv
import struct

# Connection settings
SERVER_PORT = 502
START_ADDRESS = 1001
NUM_COILS = 8  # addresses 1001-1008

# Setup CSV Logger
def setup_csv_logger():
    """Setup CSV logging system"""
    # Create logs folder if not exists
    log_dir = "logs"
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    
    # Create CSV filenames with timestamp
    timestamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    event_csv_filename = os.path.join(log_dir, f"modbus_events_{timestamp}.csv")
    
    # Create Event CSV file with headers
    with open(event_csv_filename, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['Timestamp', 'Cycle', 'Cabinet', 'Unit_ID', 'Event_Type', 
                        'Status', 'Value', 'Response_Time_ms', 'Temperature_C', 'Details'])
    
    return event_csv_filename

# CSV logging helper functions
def log_event_csv(filename, cycle, cabinet, unit_id, event_type, status, value='', 
                  response_time='', temperature='', details=''):
    """Write event to CSV file"""
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    with open(filename, 'a', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, cycle, cabinet, unit_id, event_type, status, 
                        value, response_time, temperature, details])

# Create global CSV file
event_csv_file = setup_csv_logger()
current_cycle = 0  # Global cycle counter

# Functions to generate Device ID lists for different cabinet types
def generate_device_ids_80():
    """80-channel cabinet: 11-18, 21-28, ..., 101-108"""
    device_ids = []
    for tens in range(1, 11):  # 1x, 2x, ..., 10x
        for ones in range(1, 9):  # x1-x8
            device_ids.append(tens * 10 + ones)
    return device_ids

def generate_device_ids_68():
    """68-channel cabinet: 11-18, 21-28, 31-38, 41-48, 51-58, 61-68, 71-78 and 81-86, 91-96"""
    device_ids = []
    # First set: 11-18, 21-28, 31-38, 41-48, 51-58, 61-68, 71-78 (7 groups x 8 = 56 IDs)
    for tens in range(1, 8):  # 1x-7x
        for ones in range(1, 9):  # x1-x8
            device_ids.append(tens * 10 + ones)
    # Second set: 81-86, 91-96 (2 groups x 6 = 12 IDs)
    for tens in range(8, 10):  # 8x, 9x
        for ones in range(1, 7):  # x1-x6
            device_ids.append(tens * 10 + ones)
    return device_ids

def generate_device_ids_40():
    """40-channel cabinet: 11-14, 21-24, ..., 101-104"""
    device_ids = []
    for tens in range(1, 11):  # 1x, 2x, ..., 10x
        for ones in range(1, 5):  # x1-x4
            device_ids.append(tens * 10 + ones)
    return device_ids

def generate_device_ids_60():
    """60-channel cabinet: 11-18, 21-28, 31-38 and 41-44, 51-54, 61-64, 71-74, 81-84 and 91-98, 101-108"""
    device_ids = []
    # First set: 11-18, 21-28, 31-38 (8 per group)
    for tens in range(1, 4):  # 1x, 2x, 3x
        for ones in range(1, 9):  # x1-x8
            device_ids.append(tens * 10 + ones)
    # Middle set: 41-44, 51-54, 61-64, 71-74, 81-84 (4 per group)
    for tens in range(4, 9):  # 4x, 5x, 6x, 7x, 8x
        for ones in range(1, 5):  # x1-x4
            device_ids.append(tens * 10 + ones)
    # Last set: 91-98, 101-108 (8 per group)
    for tens in range(9, 11):  # 9x, 10x
        for ones in range(1, 9):  # x1-x8
            device_ids.append(tens * 10 + ones)
    return device_ids

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

def get_device_ids_for_cabinet(cabinet_type):
    """Get Device IDs based on cabinet type"""
    if cabinet_type == 80:
        return generate_device_ids_80()
    elif cabinet_type == 68:
        return generate_device_ids_68()
    elif cabinet_type == 60:
        return generate_device_ids_60()
    elif cabinet_type == 40:
        return generate_device_ids_40()
    else:
        raise ValueError(f"Unsupported cabinet type: {cabinet_type} channels")

def broadcast_single_coil(client, address=4999, value=True, cabinet_name=''):
    """
    Broadcast wake-up packet to wake devices from standby mode
    Send small packet (2 bytes) - fire and forget
    
    Args:
        client: ModbusTcpClient object
        address: Coil address (default: 4999) - not used for wake-up
        value: Coil value (default: True) - not used for wake-up
        cabinet_name: Cabinet name for CSV logging
    """
    try:
        # Send minimal wake-up packet (2 bytes) to trigger devices out of standby
        # This is NOT a valid Modbus packet - just to wake up the bus
        wake_up_packet = b'\x00\x01'
        
        # Send directly to socket without waiting for response
        if hasattr(client, 'socket') and client.socket:
            client.socket.send(wake_up_packet)
            success = True
        else:
            # Fallback if socket not accessible
            success = False
            
    except Exception as e:
        # Log error but don't stop execution
        log_event_csv(event_csv_file, current_cycle, cabinet_name, 0, 
                     'BROADCAST_WAKE', 'ERROR', '', '', '', 
                     f'Error: {str(e)}')
        print(f"[BROADCAST ERROR] {e}")
        return
    
    # Log และแสดงผลว่าส่งไปแล้ว
    log_event_csv(event_csv_file, current_cycle, cabinet_name, 0, 
                 'BROADCAST_WAKE', 'SENT', '', '', '', 'Wake-up packet (2 bytes)')
    print(f"[BROADCAST] Sent wake-up packet (2 bytes) to trigger devices out of standby")

def read_temperature(client, unit_id, cabinet_name=''):
    """
    Read temperature from holding register address 20
    Temperature value is divided by 100 to get actual temperature
    
    Args:
        client: ModbusTcpClient object
        unit_id: Device ID
        cabinet_name: Cabinet name for CSV logging
    
    Returns:
        tuple: (success, temperature, response_time)
    """
    cmd_start = time.time()
    try:
        result = client.read_holding_registers(
            address=20,
            count=1,
            device_id=unit_id
        )
        response_time = (time.time() - cmd_start) * 1000  # Convert to ms
        
        if result.isError():
            log_event_csv(event_csv_file, current_cycle, cabinet_name, unit_id, 
                         'TEMPERATURE_READ', 'FAILED', '', f"{response_time:.2f}", '', str(result))
            print(f"[FAIL] Unit ID {unit_id}: Temperature read failed - {result} | Response: {response_time:.2f}ms")
            return False, None, response_time
        else:
            # Divide by 100 to get actual temperature
            temperature = result.registers[0] / 100.0
            log_event_csv(event_csv_file, current_cycle, cabinet_name, unit_id, 
                         'TEMPERATURE_READ', 'SUCCESS', '', f"{response_time:.2f}", f"{temperature:.2f}", '')
            print(f"[OK] Unit ID {unit_id}: Temperature = {temperature:.2f}C | Response: {response_time:.2f}ms")
            return True, temperature, response_time
            
    except Exception as e:
        response_time = (time.time() - cmd_start) * 1000
        log_event_csv(event_csv_file, current_cycle, cabinet_name, unit_id, 
                     'TEMPERATURE_READ', 'ERROR', '', f"{response_time:.2f}", '', str(e))
        print(f"[ERROR] Unit ID {unit_id}: Temperature read error - {e} | Response: {response_time:.2f}ms")
        return False, None, response_time

def write_coils_to_device(client, unit_id, values, cabinet_name=''):
    """
    Write values to multiple coils of device with timing
    
    Args:
        client: ModbusTcpClient object
        unit_id: Device ID (11-18, 21-28, ..., 101-108)
        values: list of True/False values for 8 coils
        cabinet_name: Cabinet name for CSV logging
    
    Returns:
        tuple: (success, response_time_ms)
    """
    cmd_start = time.time()
    try:
        # Write multiple coils
        result = client.write_coils(
            address=START_ADDRESS,
            values=values,
            device_id=unit_id
        )
        response_time = (time.time() - cmd_start) * 1000  # Convert to milliseconds
        value_str = str(values)
        
        if result.isError():
            log_event_csv(event_csv_file, current_cycle, cabinet_name, unit_id, 
                         'WRITE_COILS', 'FAILED', value_str, f"{response_time:.2f}", '', str(result))
            print(f"[FAIL] Unit ID {unit_id}: Write failed - {result} | Response: {response_time:.2f}ms")
            return False, response_time
        else:
            log_event_csv(event_csv_file, current_cycle, cabinet_name, unit_id, 
                         'WRITE_COILS', 'SUCCESS', value_str, f"{response_time:.2f}", '', '')
            print(f"[OK] Unit ID {unit_id}: Write success - Coils {START_ADDRESS}-{START_ADDRESS+NUM_COILS-1} = {values} | Response: {response_time:.2f}ms")
            return True, response_time
            
    except ModbusException as e:
        response_time = (time.time() - cmd_start) * 1000
        log_event_csv(event_csv_file, current_cycle, cabinet_name, unit_id, 
                     'WRITE_COILS', 'EXCEPTION', str(values), f"{response_time:.2f}", '', str(e))
        print(f"[FAIL] Unit ID {unit_id}: Modbus Exception - {e} | Response: {response_time:.2f}ms")
        return False, response_time
    except Exception as e:
        response_time = (time.time() - cmd_start) * 1000
        log_event_csv(event_csv_file, current_cycle, cabinet_name, unit_id, 
                     'WRITE_COILS', 'ERROR', str(values), f"{response_time:.2f}", '', str(e))
        print(f"[FAIL] Unit ID {unit_id}: Error - {e} | Response: {response_time:.2f}ms")
        return False, response_time

def set_all_devices_coils(client, device_ids, values, cabinet_name='', read_temp=True):
    """
    Write coil values to all devices with temperature reading
    
    Args:
        client: ModbusTcpClient object
        device_ids: list of Device IDs
        values: list of True/False values for 8 coils
        cabinet_name: Cabinet name for CSV logging
        read_temp: whether to read temperature (default: True)
    
    Returns:
        tuple: (success_count, fail_count, avg_response_time, temp_readings)
    """
    num_devices = len(device_ids)
    success_count = 0
    fail_count = 0
    response_times = []
    temp_readings = []
    
    print(f"\n{'='*60}")
    print(f"Writing to {num_devices} devices")
    print(f"Values to write: {values}")
    print(f"{'='*60}\n")
    
    for idx, unit_id in enumerate(device_ids, start=1):
        # Write coils
        success, resp_time = write_coils_to_device(client, unit_id, values, cabinet_name)
        response_times.append(resp_time)
        
        if success:
            success_count += 1
        else:
            fail_count += 1
        
        # Small delay to avoid overwhelming the network
        time.sleep(0.05)
    
    avg_response = sum(response_times) / len(response_times) if response_times else 0
    
    print(f"\n{'='*60}")
    print(f"Batch Summary:")
    print(f"  Success: {success_count}/{num_devices}")
    print(f"  Failed: {fail_count}/{num_devices}")
    print(f"  Avg Response Time: {avg_response:.2f}ms")
    if temp_readings:
        avg_temp = sum(t['temp'] for t in temp_readings) / len(temp_readings)
        print(f"  Temperature readings: {len(temp_readings)} devices | Avg: {avg_temp:.2f}C")
    print(f"{'='*60}\n")
    
    return success_count, fail_count, avg_response, temp_readings

def set_specific_device_coils(client, unit_id, values, cabinet_name=''):
    """
    Write coil values to a specific device
    
    Args:
        client: ModbusTcpClient object
        unit_id: Unit ID of device
        values: list of True/False values for 8 coils
        cabinet_name: Cabinet name for CSV logging
    """
    print(f"\n{'='*60}")
    print(f"Writing to Unit ID {unit_id}")
    print(f"Values to write: {values}")
    print(f"{'='*60}\n")
    
    # Write coils
    success, resp_time = write_coils_to_device(client, unit_id, values, cabinet_name)

def test_cabinet(cabinet, all_true, all_false, cabinet_num, total_cabinets):
    """
    Test one cabinet (send True to all IDs then send False to all IDs)
    
    Args:
        cabinet: dict of cabinet info (name, ip, type)
        all_true: list of True values for coils
        all_false: list of False values for coils
        cabinet_num: current cabinet number
        total_cabinets: total number of cabinets
    
    Returns:
        dict: cabinet test statistics (including elapsed time)
    """
    start_time = time.time()
    cabinet_name = cabinet['name']
    
    print(f"\n{'#'*70}")
    print(f"# [{cabinet_num}/{total_cabinets}] {cabinet_name} - IP: {cabinet['ip']} - {cabinet['type']} channels")
    print(f"{'#'*70}")
    
    # Log connection attempt
    log_event_csv(event_csv_file, current_cycle, cabinet_name, '', 'CONNECTION_START', 
                 'ATTEMPTING', '', '', '', f"IP: {cabinet['ip']}:{SERVER_PORT}")
    
    # Generate Device IDs for this cabinet
    device_ids = get_device_ids_for_cabinet(cabinet['type'])
    print(f"Device IDs: {device_ids[:5]} ... {device_ids[-5:]} (Total: {len(device_ids)} IDs)\n")
    
    # Create Modbus TCP Client
    print(f"Connecting to {cabinet['ip']}:{SERVER_PORT}...")
    client = ModbusTcpClient(cabinet['ip'], port=SERVER_PORT, timeout=3.0)
    
    # Connect to server (max 2 attempts)
    connection_attempts = 0
    max_attempts = 2
    connected = False
    
    while connection_attempts < max_attempts and not connected:
        connection_attempts += 1
        
        if client.connect():
            connected = True
            print(f"[CONNECTED] Connection successful!\n")
            log_event_csv(event_csv_file, current_cycle, cabinet_name, '', 'CONNECTION', 
                         'SUCCESS', '', '', '', f"Attempt {connection_attempts}/{max_attempts}")
        else:
            if connection_attempts < max_attempts:
                print(f"[RETRY] Connection attempt {connection_attempts} failed - waiting 20 seconds before retry...")
                log_event_csv(event_csv_file, current_cycle, cabinet_name, '', 'CONNECTION', 
                             'RETRY', '', '', '', f"Attempt {connection_attempts} failed")
                time.sleep(20)
                print(f"Retrying connection (attempt {connection_attempts + 1}/{max_attempts})...")
            else:
                elapsed_time = time.time() - start_time
                print(f"[FAILED] Unable to connect to {cabinet['ip']}:{SERVER_PORT} after {max_attempts} attempts - Skipping")
                log_event_csv(event_csv_file, current_cycle, cabinet_name, '', 'CONNECTION', 
                             'FAILED', '', '', '', f"All {max_attempts} attempts failed")
                return {'cabinet': cabinet_name, 'connected': False, 'success': 0, 'fail': 0, 
                       'elapsed_time': elapsed_time, 'avg_response': 0, 'temp_data': []}
    
    if not connected:
        elapsed_time = time.time() - start_time
        return {'cabinet': cabinet_name, 'connected': False, 'success': 0, 'fail': 0, 
               'elapsed_time': elapsed_time, 'avg_response': 0, 'temp_data': []}
    
    try:
        # Set True to all Device IDs
        print(">>> Step 1: Writing TRUE to all Device IDs")
        time.sleep(2)  # Delay before starting batch
        success_true, fail_true, avg_resp_true, temp_true = set_all_devices_coils(
            client, device_ids, all_true, cabinet_name)
        
        # Set False to all Device IDs
        print(">>> Step 2: Writing FALSE to all Device IDs")
        time.sleep(2)  # Delay before starting batch
        success_false, fail_false, avg_resp_false, temp_false = set_all_devices_coils(
            client, device_ids, all_false, cabinet_name)
        
        elapsed_time = time.time() - start_time
        avg_response = (avg_resp_true + avg_resp_false) / 2
        temp_data = temp_true + temp_false
        
        print(f"Elapsed time: {elapsed_time:.2f} seconds | Avg Response: {avg_response:.2f}ms")
        
        return {
            'cabinet': cabinet_name,
            'connected': True,
            'success': success_true + success_false,
            'fail': fail_true + fail_false,
            'elapsed_time': elapsed_time,
            'avg_response': avg_response,
            'temp_data': temp_data
        }
    
    finally:
        # Close connection
        client.close()
        print(f"Connection closed for {cabinet_name}\n")
        log_event_csv(event_csv_file, current_cycle, cabinet_name, '', 'CONNECTION', 
                     'CLOSED', '', '', '', '')

def main():
    """Main function - Test all cabinets (1 Cycle = test all cabinets)"""
    
    global current_cycle
    program_start_time = datetime.now()
    
    print("="*70)
    print(" Modbus TCP Multi-Cabinet Tester")
    print("="*70)
    print(f"\nTotal Cabinets: {len(CABINETS)}")
    print(f"1 Cycle = Test all cabinets (each cabinet: write True and False to all IDs)")
    print(f"Event CSV: {event_csv_file}")
    print(f"\nCabinet List:")
    for idx, cab in enumerate(CABINETS, 1):
        print(f"   {idx}. {cab['name']:12} - {cab['ip']:15} - {cab['type']:2} channels")
    print()
    
    # Log program start to CSV
    log_event_csv(event_csv_file, 0, 'SYSTEM', '', 'PROGRAM_START', 'INFO', '', '', '', 
                 f"Total Cabinets: {len(CABINETS)}")
    
    # Prepare values for coils
    all_true = [True] * NUM_COILS
    all_false = [False] * NUM_COILS
    
    try:
        # ===== Continuous loop =====
        print("\nStarting continuous loop (Press Ctrl+C to stop)\n")
        
        loop_count = 0
        total_stats = {'success': 0, 'fail': 0, 'disconnected': 0}
        
        while True:
            loop_count += 1
            current_cycle = loop_count  # Update global cycle counter
            cycle_start_time = time.time()
            cycle_start_datetime = datetime.now()
            
            print(f"\n{'='*70}")
            print(f"CYCLE #{loop_count} - Testing all cabinets ({len(CABINETS)} cabinets)")
            print(f"{'='*70}\n")
            
            # Log cycle start
            log_event_csv(event_csv_file, loop_count, 'SYSTEM', '', 'CYCLE_START', 'INFO', '', '', '', 
                         f"Testing {len(CABINETS)} cabinets")
            
            loop_stats = {'success': 0, 'fail': 0, 'disconnected': 0}
            cabinet_times = []
            
            # Test all cabinets in order
            for idx, cabinet in enumerate(CABINETS, 1):
                result = test_cabinet(cabinet, all_true, all_false, idx, len(CABINETS))
                
                if result['connected']:
                    loop_stats['success'] += result['success']
                    loop_stats['fail'] += result['fail']
                else:
                    loop_stats['disconnected'] += 1
                
                # Store cabinet timing data
                cabinet_times.append({
                    'name': result['cabinet'],
                    'time': result['elapsed_time'],
                    'connected': result['connected'],
                    'avg_response': result.get('avg_response', 0),
                    'temp_data': result.get('temp_data', [])
                })
                
                time.sleep(0.5)
            
            # Calculate total cycle time
            cycle_elapsed_time = time.time() - cycle_start_time
            cycle_end_datetime = datetime.now()
            
            # Update total statistics
            total_stats['success'] += loop_stats['success']
            total_stats['fail'] += loop_stats['fail']
            total_stats['disconnected'] += loop_stats['disconnected']
            
            # Display cycle summary
            print(f"\n{'='*70}")
            print(f"CYCLE #{loop_count} SUMMARY (Completed all {len(CABINETS)} cabinets)")
            print(f"   Success: {loop_stats['success']} commands")
            print(f"   Failed: {loop_stats['fail']} commands")
            print(f"   Disconnected: {loop_stats['disconnected']} cabinets")
            print(f"\n   Cabinet test times:")
            for cab_time in cabinet_times:
                status = "[OK]" if cab_time['connected'] else "[FAIL]"
                temp_data = cab_time['temp_data']
                avg_temp = sum(t['temp'] for t in temp_data) / len(temp_data) if temp_data else 0
                temp_str = f"Temp: {avg_temp:.2f}C" if temp_data else "Temp: N/A"
                print(f"      {status} {cab_time['name']:12} - {cab_time['time']:6.2f}s | Resp: {cab_time['avg_response']:6.2f}ms | {temp_str}")
            print(f"\n   Total cycle time: {cycle_elapsed_time:.2f} seconds ({cycle_elapsed_time/60:.2f} minutes)")
            print(f"{'='*70}\n")
            
            # Log cycle summary
            log_event_csv(event_csv_file, loop_count, 'SYSTEM', '', 'CYCLE_SUMMARY', 'INFO', 
                         f"Success:{loop_stats['success']},Failed:{loop_stats['fail']}", 
                         '', '', f"Time: {cycle_elapsed_time:.2f}s, Disconnected: {loop_stats['disconnected']}")
            
            print(f"Waiting 3 seconds before next cycle...\n")
            time.sleep(3)
        
    except KeyboardInterrupt:
        program_end_time = datetime.now()
        total_runtime = program_end_time - program_start_time
        
        print("\n\nProgram stopped by user")
        print(f"\n{'='*70}")
        print(f"Overall Summary ({loop_count} Cycles):")
        print(f"   Completed Cycles: {loop_count}")
        print(f"   Total Success: {total_stats['success']} commands")
        print(f"   Total Failed: {total_stats['fail']} commands")
        print(f"   Connection Failures: {total_stats['disconnected']} times")
        print(f"{'='*70}")
        
        # Log program end
        log_event_csv(event_csv_file, loop_count, 'SYSTEM', '', 'PROGRAM_END', 'INFO', 
                     f"Cycles:{loop_count}", '', '', 
                     f"Runtime: {total_runtime}, Success: {total_stats['success']}, Failed: {total_stats['fail']}")
    
    print("\nProgram terminated")
    # Final log entry
    log_event_csv(event_csv_file, current_cycle, 'SYSTEM', '', 'PROGRAM_TERMINATED', 'INFO', '', '', '', '')

if __name__ == "__main__":
    main()
