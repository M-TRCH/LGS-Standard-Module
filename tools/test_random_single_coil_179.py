"""
Modbus TCP Random Single Coil Tester (Single Cabinet - 3 Floors)
Cabinet IP: 192.168.0.179:502
Floors: 11-18, 21-28, 31-38
Each cycle: Set RGB Color -> Write True -> Read Registers -> Write False
Consecutive cycles must not repeat the same floor.
"""

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException
import time
from datetime import datetime
import os
import csv
import random
import colorsys

# Connection settings
SERVER_IP = "192.168.0.179"
SERVER_PORT = 502
START_ADDRESS = 1001
NUM_COILS = 8  # addresses 1001-1008

# Floor configuration (3 floors, channel 5 only)
FLOORS = {
    1: [15],  # Floor 1, channel 5
    2: [25],  # Floor 2, channel 5
    3: [35],  # Floor 3, channel 5
}

# Fixed test sequence: 15 -> 25 -> 35 -> 15 -> ...
FLOOR_SEQUENCE = [15, 25, 35]
FLOOR_LABELS = {15: 1, 25: 2, 35: 3}

# Generate 16 maximally distinct RGB preset colors using HSV
def generate_color_presets():
    """Generate 16 maximally distinct 8-bit RGB colors via evenly spaced HSV hues"""
    colors = []
    for i in range(16):
        h = i / 16.0  # Hue: 0.0 ~ 0.9375
        s = 1.0       # Full saturation
        v = 1.0       # Full brightness
        r, g, b = colorsys.hsv_to_rgb(h, s, v)
        colors.append((int(r * 255), int(g * 255), int(b * 255)))
    return colors

COLOR_PRESETS = generate_color_presets()
# Result: 16 colors evenly spaced around the hue wheel
# [0]  (255,   0,   0)  Red
# [1]  (255,  95,   0)  Orange
# [2]  (255, 191,   0)  Amber
# [3]  (223, 255,   0)  Yellow-Green
# [4]  (127, 255,   0)  Chartreuse
# [5]  ( 31, 255,   0)  Green
# [6]  (  0, 255,  63)  Spring Green
# [7]  (  0, 255, 159)  Turquoise
# [8]  (  0, 255, 255)  Cyan
# [9]  (  0, 159, 255)  Sky Blue
# [10] (  0,  63, 255)  Blue
# [11] ( 31,   0, 255)  Indigo
# [12] (127,   0, 255)  Violet
# [13] (223,   0, 255)  Purple
# [14] (255,   0, 191)  Magenta
# [15] (255,   0,  95)  Rose

# Setup CSV Logger
def setup_csv_logger():
    """Setup CSV logging system"""
    log_dir = "logs"
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)

    timestamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    event_csv_filename = os.path.join(log_dir, f"random_test_179_{timestamp}.csv")

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

def get_led_rgb_registers(coil_address):
    """Get RGB register addresses for a given coil address.
    LED1 (coil 1001) -> R=111, G=112, B=113
    LED2 (coil 1002) -> R=121, G=122, B=123
    ...
    LED8 (coil 1008) -> R=181, G=182, B=183
    """
    led_num = coil_address - 1000  # 1-8
    base = led_num * 10 + 101      # 111, 121, ..., 181
    return base, base + 1, base + 2

def write_rgb_color(client, unit_id, coil_address, rgb, cabinet_name, csv_file, cycle):
    """Write RGB color to LED coil addresses using write_coils (FC15)
    LED1 (coil 1001) -> coils 111,112,113 = [R,G,B] as bool
    """
    r_addr, g_addr, b_addr = get_led_rgb_registers(coil_address)
    r, g, b = rgb
    led_num = coil_address - 1000
    cmd_start = time.time()
    try:
        result = client.write_coils(
            address=r_addr,
            values=[bool(r), bool(g), bool(b)],
            device_id=unit_id
        )
        response_time = (time.time() - cmd_start) * 1000

        if result.isError():
            log_event_csv(csv_file, cycle, cabinet_name, unit_id, r_addr,
                          'WRITE_RGB', 'FAILED', f"R={bool(r)},G={bool(g)},B={bool(b)}",
                          f"{response_time:.2f}",
                          f"LED{led_num} Coil {r_addr}-{b_addr}: {result}")
            print(f"    [RGB]  FAIL  {response_time:.1f}ms  -> {result}")
            return False, response_time
        else:
            log_event_csv(csv_file, cycle, cabinet_name, unit_id, r_addr,
                          'WRITE_RGB', 'SUCCESS', f"R={bool(r)},G={bool(g)},B={bool(b)}",
                          f"{response_time:.2f}",
                          f"LED{led_num} Coil {r_addr}-{b_addr}")
            print(f"    [RGB]  OK    {response_time:.1f}ms")
            return True, response_time

    except Exception as e:
        response_time = (time.time() - cmd_start) * 1000
        log_event_csv(csv_file, cycle, cabinet_name, unit_id, r_addr,
                      'WRITE_RGB', 'ERROR', f"R={bool(r)},G={bool(g)},B={bool(b)}",
                      f"{response_time:.2f}", str(e))
        print(f"    [RGB]  ERR   {response_time:.1f}ms  -> {e}")
        return False, response_time

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
            print(f"    [{'ON ' if value else 'OFF'}]  FAIL  {response_time:.1f}ms  -> {result}")
            return False, response_time
        else:
            log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                          'WRITE_SINGLE_COIL', 'SUCCESS', str(value),
                          f"{response_time:.2f}", '')
            print(f"    [{'ON ' if value else 'OFF'}]  OK    {response_time:.1f}ms")
            return True, response_time

    except Exception as e:
        response_time = (time.time() - cmd_start) * 1000
        log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                      'WRITE_SINGLE_COIL', 'ERROR', str(value),
                      f"{response_time:.2f}", str(e))
        print(f"    [{'ON ' if value else 'OFF'}]  ERR   {response_time:.1f}ms  -> {e}")
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
            print(f"  [FAIL] Unit {unit_id}, Reg {address}: Read failed - {result} | {response_time:.2f}ms")
            return False, None, response_time
        else:
            value = result.registers[0]
            log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                          'READ_REGISTER', 'SUCCESS', str(value),
                          f"{response_time:.2f}", '')
            print(f"  [OK] Unit {unit_id}, Reg {address}: Value = {value} | {response_time:.2f}ms")
            return True, value, response_time

    except Exception as e:
        response_time = (time.time() - cmd_start) * 1000
        log_event_csv(csv_file, cycle, cabinet_name, unit_id, address,
                      'READ_REGISTER', 'ERROR', '',
                      f"{response_time:.2f}", str(e))
        print(f"  [ERROR] Unit {unit_id}, Reg {address}: Read error - {e} | {response_time:.2f}ms")
        return False, None, response_time

def run_random_test_cycle(cycle_num, csv_file, client, floor_index, color_queue):
    """Run one test cycle in fixed floor sequence. Returns success."""
    cabinet_name = f"Cabinet({SERVER_IP})"

    # 1. Fixed floor sequence: 15 -> 25 -> 35
    unit_id = FLOOR_SEQUENCE[floor_index]
    floor = FLOOR_LABELS[unit_id]

    # 2. Random coil address (1001-1008)
    coil_address = random.randint(START_ADDRESS, START_ADDRESS + NUM_COILS - 1)
    led_num = coil_address - 1000

    # 3. Next color from shuffled queue (no repeats until all 16 used)
    if not color_queue:
        indices = list(range(len(COLOR_PRESETS)))
        random.shuffle(indices)
        color_queue.extend(indices)
    color_idx = color_queue.pop(0)
    rgb = COLOR_PRESETS[color_idx]

    print(f"\nCycle #{cycle_num:>4}  |  Floor:{floor} ID:{unit_id}  |  LED{led_num} Addr:{coil_address}  |  Color:#{color_idx:02d} RGB{rgb}")

    # Log cycle start
    log_event_csv(csv_file, cycle_num, cabinet_name, unit_id, coil_address,
                  'CYCLE_START', 'INFO', '', '',
                  f"Floor:{floor}, Unit:{unit_id}, Addr:{coil_address}, Color:RGB{rgb}")

    try:
        success_rgb, time_rgb = write_rgb_color(client, unit_id, coil_address, rgb,
                                                cabinet_name, csv_file, cycle_num)

        time.sleep(0.1)

        success_on, time_on = write_single_coil(client, unit_id, coil_address, True,
                                                cabinet_name, csv_file, cycle_num)

        time.sleep(0.5)

        success_off, time_off = write_single_coil(client, unit_id, coil_address, False,
                                                  cabinet_name, csv_file, cycle_num)

        # Summary
        total_success = success_rgb and success_on and success_off
        times = [time_rgb, time_on, time_off]
        avg_time = sum(times) / len(times)

        result_str = "SUCCESS" if total_success else "FAILED "
        print(f"  -> {result_str}  |  Avg: {avg_time:.1f}ms")

        log_event_csv(csv_file, cycle_num, cabinet_name, unit_id, coil_address,
                      'CYCLE_END', 'SUCCESS' if total_success else 'FAILED', '',
                      f"{avg_time:.2f}",
                      f"Floor:{floor}, Color:RGB{rgb}")

        return total_success

    except Exception as e:
        log_event_csv(csv_file, cycle_num, cabinet_name, unit_id, coil_address,
                      'CYCLE_END', 'ERROR', '', '', str(e))
        print(f"  -> ERROR: {e}")
        return False

def main():
    """Main function"""
    csv_file = setup_csv_logger()

    print("=" * 70)
    print(" Modbus TCP Random Single Coil Tester (Single Cabinet - 3 Floors)")
    print("=" * 70)
    print(f"\n  Cabinet IP: {SERVER_IP}:{SERVER_PORT}")
    print(f"  Floors: 3 (Channel 5 only: IDs 15, 25, 35)")
    print(f"  Total Unit IDs: {sum(len(ids) for ids in FLOORS.values())}")
    print(f"  Coil Range: {START_ADDRESS}-{START_ADDRESS + NUM_COILS - 1}")
    print(f"  Color Presets: {len(COLOR_PRESETS)} colors")
    print(f"  Event CSV: {csv_file}")
    print(f"\n  Color Presets:")
    for i, c in enumerate(COLOR_PRESETS):
        print(f"    #{i:2d}: RGB({c[0]:3d}, {c[1]:3d}, {c[2]:3d})")
    print(f"\n  LED Register Mapping:")
    for led in range(1, 9):
        base = led * 10 + 101
        print(f"    LED{led} (Coil {1000+led}): R={base}, G={base+1}, B={base+2}")
    print(f"\n  ** Auto-pause: 01:45-02:15 daily (New log file at 02:15)")
    print()

    # Log program start
    log_event_csv(csv_file, 0, 'SYSTEM', '', '', 'PROGRAM_START', 'INFO', '', '',
                  f"IP:{SERVER_IP}, Floors:3, Colors:{len(COLOR_PRESETS)}")

    # Connect
    print(f"Connecting to {SERVER_IP}:{SERVER_PORT}...")
    client = ModbusTcpClient(SERVER_IP, port=SERVER_PORT, timeout=3.0)

    if not client.connect():
        print(f"[FAILED] Connection failed!")
        log_event_csv(csv_file, 0, 'SYSTEM', '', '', 'CONNECTION', 'FAILED', '', '',
                      'Unable to connect')
        return

    print(f"[CONNECTED] Connection successful!")
    log_event_csv(csv_file, 0, 'SYSTEM', '', '', 'CONNECTION', 'SUCCESS', '', '', '')

    try:
        print("\nStarting continuous random testing (Press Ctrl+C to stop)\n")

        cycle_count = 0
        success_count = 0
        fail_count = 0
        floor_index = 0
        color_queue = []
        program_start_time = datetime.now()

        while True:
            # Check pause window (01:45-02:15)
            current_time = datetime.now().time()
            pause_start = datetime.strptime("01:45", "%H:%M").time()
            pause_end = datetime.strptime("02:15", "%H:%M").time()

            if pause_start <= current_time < pause_end:
                log_event_csv(csv_file, cycle_count, 'SYSTEM', '', '', 'DAILY_PAUSE', 'INFO',
                              '', '', f"Pausing at {current_time.strftime('%H:%M:%S')}")
                print(f"\n{'='*70}")
                print(f"[PAUSE] Daily maintenance window (01:45-02:15)")
                print(f"Current time: {current_time.strftime('%H:%M:%S')}")
                print(f"Will resume at 02:15 with new log file")
                print(f"{'='*70}\n")

                client.close()

                while datetime.now().time() < pause_end:
                    time.sleep(10)

                csv_file = setup_csv_logger()
                success_count = 0
                fail_count = 0
                floor_index = 0
                color_queue = []
                program_start_time = datetime.now()

                # Reconnect
                client = ModbusTcpClient(SERVER_IP, port=SERVER_PORT, timeout=3.0)
                if not client.connect():
                    print(f"[FAILED] Reconnection failed after pause!")
                    return

                print(f"\n{'='*70}")
                print(f"[RESUME] Resumed at {datetime.now().time().strftime('%H:%M:%S')}")
                print(f"New log file: {csv_file}")
                print(f"{'='*70}\n")

                log_event_csv(csv_file, 0, 'SYSTEM', '', '', 'DAILY_RESUME', 'INFO',
                              '', '', f"Resumed at {datetime.now().time().strftime('%H:%M:%S')}")

            cycle_count += 1

            success = run_random_test_cycle(cycle_count, csv_file, client, floor_index, color_queue)
            floor_index = (floor_index + 1) % len(FLOOR_SEQUENCE)

            if success:
                success_count += 1
            else:
                fail_count += 1

            rate = (success_count / cycle_count * 100) if cycle_count > 0 else 0
            print(f"  Stats: {success_count}/{cycle_count} ({rate:.1f}%)  |  Next in 500ms...")
            time.sleep(0.5)

    except KeyboardInterrupt:
        program_end_time = datetime.now()
        total_runtime = program_end_time - program_start_time

        print("\n\nProgram stopped by user")
        print(f"\n{'='*70}")
        print(f"Overall Summary:")
        print(f"   Total Cycles: {cycle_count}")
        print(f"   Success: {success_count}")
        print(f"   Failed: {fail_count}")
        if cycle_count > 0:
            print(f"   Success Rate: {(success_count/cycle_count*100):.1f}%")
        print(f"   Runtime: {total_runtime}")
        print(f"{'='*70}")

        log_event_csv(csv_file, cycle_count, 'SYSTEM', '', '', 'PROGRAM_END', 'INFO',
                      f"Cycles:{cycle_count}", '',
                      f"Runtime: {total_runtime}, Success: {success_count}, Failed: {fail_count}")

    finally:
        client.close()
        log_event_csv(csv_file, cycle_count if 'cycle_count' in dir() else 0,
                      'SYSTEM', '', '', 'CONNECTION', 'CLOSED', '', '', '')

    print("\nProgram terminated")

if __name__ == "__main__":
    main()
