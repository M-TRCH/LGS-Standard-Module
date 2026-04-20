"""
Modbus TCP Heavy-Duty Grid Tester
Grid: 7 rows x 8 columns (Unit IDs: 11-18, 21-28, ..., 71-78)
One cycle = traverse all 56 units in two phases:
  Phase 1.1: Write RGB -> LED+Latch ON -> Read Latch Status  (all 56 units)
  Phase 1.2: Read Latch Status -> LED OFF                    (all 56 units)
Seam = row boundary (e.g. ID 18 -> 21, ID 58 -> 61)
"""

from pymodbus.client import ModbusTcpClient
import time
from datetime import datetime
import os
import csv
import random
import colorsys

# ─── Connection ──────────────────────────────────────────────────────────────
SERVER_IP = "192.168.6.133"
SERVER_PORT = 502

# ─── Grid ────────────────────────────────────────────────────────────────────
ROWS = 7        # rows 1-7
COLS = 8        # cols 1-8  -> unit ID = row*10 + col

# ─── Timing (seconds) ───────────────────────────────────────────────────────
DELAY_ACTION    = 0.050     # 50 ms  between actions within a sub-cycle
DELAY_SUBCYCLE  = 0.600     # 600 ms between sub-cycles
DELAY_SEAM      = 0.800     # 800 ms at row boundary (seam)
DELAY_CYCLE     = 3.000     # 3000 ms between cycles

# ─── Modbus addresses ───────────────────────────────────────────────────────
COIL_LED_LATCH_BASE  = 1021     # 1021-1028: LED N + Latch (ON, auto-reset)
COIL_LED_ENABLE_BASE = 1001     # 1001-1008: LED N Enable  (OFF)
REG_LATCH_STATUS     = 40       # Time After Unlock (seconds, read-only)
REG_LED_CONFIG_BASE  = 110      # LED 1 config start; stride = 10 per LED
#   Offset +0 = Brightness, +1 = Red, +2 = Green, +3 = Blue

# ─── Color presets (16 HSV hues) ────────────────────────────────────────────
def generate_color_presets():
    colors = []
    for i in range(16):
        r, g, b = colorsys.hsv_to_rgb(i / 16.0, 1.0, 1.0)
        colors.append((int(r * 255), int(g * 255), int(b * 255)))
    return colors

COLOR_PRESETS = generate_color_presets()


# ─── CSV logger ─────────────────────────────────────────────────────────────
def setup_csv_logger():
    log_dir = "logs"
    os.makedirs(log_dir, exist_ok=True)
    ts = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    path = os.path.join(log_dir, f"heavy_duty_{ts}.csv")
    with open(path, 'w', newline='', encoding='utf-8') as f:
        csv.writer(f).writerow([
            'Timestamp', 'Cycle', 'SubCycle', 'Row', 'Col', 'Unit_ID',
            'LED', 'Action', 'Status', 'Value', 'Response_ms', 'Details'])
    return path

def log_csv(path, cycle, sc, row, col, uid, led, action, status,
            value='', ms='', details=''):
    ts = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
    with open(path, 'a', newline='', encoding='utf-8') as f:
        csv.writer(f).writerow([
            ts, cycle, sc, row, col, uid, led, action, status, value, ms, details])


# ─── Low-level Modbus helpers ───────────────────────────────────────────────
def mb_write_registers(client, uid, address, values):
    """Write multiple holding registers (FC16). Returns (ok, ms, err)."""
    t0 = time.time()
    try:
        res = client.write_registers(address=address, values=values, device_id=uid)
        ms = (time.time() - t0) * 1000
        if res.isError():
            return False, ms, str(res)
        return True, ms, ''
    except Exception as e:
        return False, (time.time() - t0) * 1000, str(e)


def mb_write_coil(client, uid, address, value):
    """Write single coil (FC05). Returns (ok, ms, err)."""
    t0 = time.time()
    try:
        res = client.write_coil(address=address, value=value, device_id=uid)
        ms = (time.time() - t0) * 1000
        if res.isError():
            return False, ms, str(res)
        return True, ms, ''
    except Exception as e:
        return False, (time.time() - t0) * 1000, str(e)


def mb_read_register(client, uid, address):
    """Read single holding register (FC03). Returns (ok, value, ms, err)."""
    t0 = time.time()
    try:
        res = client.read_holding_registers(address=address, count=1, device_id=uid)
        ms = (time.time() - t0) * 1000
        if res.isError():
            return False, None, ms, str(res)
        return True, res.registers[0], ms, ''
    except Exception as e:
        return False, None, (time.time() - t0) * 1000, str(e)


# ─── Phase 1.1: Write RGB + LED+Latch ON + Read Latch ───────────────────────
def run_phase1(client, csv_file, cycle, sc, row, col, uid, led, rgb):
    """
    Phase 1.1 (3 actions):
    1) Write RGB to holding registers
    2) LED+Latch ON  (coil 1021-1028)
    3) Read latch status (reg 40)
    Returns (all_ok, total_comm_ms)
    """
    r, g, b = rgb
    ok_all = True
    t_sum = 0.0

    def tag(ok):
        return 'OK' if ok else 'FAIL'

    # ── Action 1: Write RGB ──────────────────────────────────────────────
    reg_r = REG_LED_CONFIG_BASE + (led - 1) * 10 + 1   # e.g. LED1 -> 111
    ok, ms, err = mb_write_registers(client, uid, reg_r, [r, g, b])
    t_sum += ms
    ok_all &= ok
    log_csv(csv_file, cycle, sc, row, col, uid, led,
            'WRITE_RGB', tag(ok), f"({r},{g},{b})", f"{ms:.1f}", err)
    s1 = f"RGB({r:3d},{g:3d},{b:3d})={tag(ok):4s} {ms:5.1f}ms"

    time.sleep(DELAY_ACTION)

    # ── Action 2: LED+Latch ON ───────────────────────────────────────────
    coil_on = COIL_LED_LATCH_BASE + led - 1
    ok, ms, err = mb_write_coil(client, uid, coil_on, True)
    t_sum += ms
    ok_all &= ok
    log_csv(csv_file, cycle, sc, row, col, uid, led,
            'LED_LATCH_ON', tag(ok), coil_on, f"{ms:.1f}", err)
    s2 = f"ON({coil_on})={tag(ok):4s} {ms:5.1f}ms"

    time.sleep(DELAY_ACTION)

    # ── Action 3: Read latch status ──────────────────────────────────────
    ok, val, ms, err = mb_read_register(client, uid, REG_LATCH_STATUS)
    t_sum += ms
    ok_all &= ok
    v_str = str(val) if val is not None else '-'
    log_csv(csv_file, cycle, sc, row, col, uid, led,
            'READ_LATCH', tag(ok), v_str, f"{ms:.1f}", err)
    s3 = f"Latch={v_str:>5s} {tag(ok):4s} {ms:5.1f}ms"

    result = 'OK' if ok_all else 'FAIL'
    print(f"  {s1} | {s2} | {s3}  [{result} {t_sum:.0f}ms]")

    return ok_all, t_sum


# ─── Phase 1.2: Read Latch + LED OFF ────────────────────────────────────────
def run_phase2(client, csv_file, cycle, sc, row, col, uid, led):
    """
    Phase 1.2 (2 actions):
    1) Read latch status (reg 40)
    2) LED OFF (coil 1001-1008 = False)
    Returns (all_ok, total_comm_ms)
    """
    ok_all = True
    t_sum = 0.0

    def tag(ok):
        return 'OK' if ok else 'FAIL'

    # ── Action 1: Read latch status ──────────────────────────────────────
    ok, val, ms, err = mb_read_register(client, uid, REG_LATCH_STATUS)
    t_sum += ms
    ok_all &= ok
    v_str = str(val) if val is not None else '-'
    log_csv(csv_file, cycle, sc, row, col, uid, led,
            'READ_LATCH', tag(ok), v_str, f"{ms:.1f}", err)
    s1 = f"Latch={v_str:>5s} {tag(ok):4s} {ms:5.1f}ms"

    time.sleep(DELAY_ACTION)

    # ── Action 2: LED OFF ────────────────────────────────────────────────
    coil_off = COIL_LED_ENABLE_BASE + led - 1
    ok, ms, err = mb_write_coil(client, uid, coil_off, False)
    t_sum += ms
    ok_all &= ok
    log_csv(csv_file, cycle, sc, row, col, uid, led,
            'LED_OFF', tag(ok), coil_off, f"{ms:.1f}", err)
    s2 = f"OFF({coil_off})={tag(ok):4s} {ms:5.1f}ms"

    result = 'OK' if ok_all else 'FAIL'
    print(f"  {s1} | {s2}  [{result} {t_sum:.0f}ms]")

    return ok_all, t_sum


# ─── Daily pause check ──────────────────────────────────────────────────────
PAUSE_START = datetime.strptime("01:45", "%H:%M").time()
PAUSE_END   = datetime.strptime("02:15", "%H:%M").time()


def check_daily_pause(client, csv_file, cycle):
    """Pause 01:45-02:15. Returns (client, csv_file) — may reconnect & new log."""
    now = datetime.now().time()
    if not (PAUSE_START <= now < PAUSE_END):
        return client, csv_file

    print(f"\n[PAUSE] 01:45-02:15 maintenance window — closing connection")
    log_csv(csv_file, cycle, 0, '', '', '', '', 'PAUSE', 'INFO')
    client.close()

    while datetime.now().time() < PAUSE_END:
        time.sleep(10)

    csv_file = setup_csv_logger()
    client = ModbusTcpClient(SERVER_IP, port=SERVER_PORT, timeout=3.0)
    if not client.connect():
        print("[FAIL] Reconnect failed after pause")
        raise SystemExit(1)

    print(f"[RESUME] {datetime.now().strftime('%H:%M:%S')}  New log: {csv_file}\n")
    log_csv(csv_file, 0, 0, '', '', '', '', 'RESUME', 'OK')
    return client, csv_file


# ─── Main ────────────────────────────────────────────────────────────────────
def main():
    csv_file = setup_csv_logger()

    print("=" * 90)
    print("  Modbus TCP Heavy-Duty Grid Tester")
    print("=" * 90)
    print(f"  Server     : {SERVER_IP}:{SERVER_PORT}")
    print(f"  Grid       : {ROWS} rows x {COLS} cols = {ROWS * COLS} units/cycle")
    print(f"  Unit IDs   : 11-18, 21-28, 31-38, 41-48, 51-58, 61-68, 71-78")
    print(f"  Phase 1.1  : WriteRGB -> LED+LatchON(1021-1028) -> ReadLatch(40)")
    print(f"  Phase 1.2  : ReadLatch(40) -> LEDoff(1001-1008)")
    print(f"  Delays     : Action {DELAY_ACTION*1000:.0f}ms | Sub-cycle {DELAY_SUBCYCLE*1000:.0f}ms"
          f" | Seam {DELAY_SEAM*1000:.0f}ms | Cycle {DELAY_CYCLE*1000:.0f}ms")
    print(f"  Colors     : {len(COLOR_PRESETS)} presets")
    print(f"  Log        : {csv_file}")
    print(f"  Daily pause: 01:45-02:15\n")

    # Connect
    print(f"Connecting to {SERVER_IP}:{SERVER_PORT}...", end=' ')
    client = ModbusTcpClient(SERVER_IP, port=SERVER_PORT, timeout=3.0)
    if not client.connect():
        print("FAILED")
        return
    print("OK\n")
    log_csv(csv_file, 0, 0, '', '', '', '', 'CONNECT', 'OK', '', '', SERVER_IP)

    cycle = 0
    total_ok = 0
    total_fail = 0
    color_queue = []
    program_start = datetime.now()

    try:
        while True:
            client, csv_file = check_daily_pause(client, csv_file, cycle)

            cycle += 1
            cycle_t0 = time.time()
            cycle_ok = 0
            cycle_fail = 0
            sc = 0  # sub-cycle counter within this cycle

            print(f"{'='*90}")
            print(f"CYCLE #{cycle:<5d}  {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
                  f"  |  {ROWS*COLS} units x 2 phases")
            print(f"{'='*90}")
            log_csv(csv_file, cycle, 0, '', '', '', '', 'CYCLE_START', 'INFO')

            # ── Phase 1.1: Write RGB + LED+Latch ON + Read Latch ────────
            unit_plan = []  # store (row, col, uid, led) for phase 1.2

            print(f"\n  Phase 1.1  WriteRGB -> LED+LatchON -> ReadLatch")
            print(f"  {'='*84}")
            log_csv(csv_file, cycle, 0, '', '', '', '', 'PHASE_1.1_START', 'INFO')

            for row in range(1, ROWS + 1):
                row_t0 = time.time()
                row_ok = 0

                print(f"\n  Row {row}  (ID {row}1 - {row}8)")
                print(f"  {'-'*84}")

                for col in range(1, COLS + 1):
                    sc += 1
                    uid = row * 10 + col
                    led = random.randint(1, 8)

                    # Next color from shuffled queue
                    if not color_queue:
                        q = list(range(len(COLOR_PRESETS)))
                        random.shuffle(q)
                        color_queue.extend(q)
                    rgb = COLOR_PRESETS[color_queue.pop(0)]

                    unit_plan.append((row, col, uid, led))

                    print(f"  [{sc:>3d}] ID:{uid:2d} L{led}", end='  ')

                    ok, _ = run_phase1(client, csv_file, cycle, sc,
                                       row, col, uid, led, rgb)
                    if ok:
                        row_ok += 1
                        cycle_ok += 1
                        total_ok += 1
                    else:
                        cycle_fail += 1
                        total_fail += 1

                    if col < COLS:
                        time.sleep(DELAY_SUBCYCLE)

                row_ms = (time.time() - row_t0) * 1000
                print(f"  Row {row} done: {row_ok}/{COLS} OK  ({row_ms:.0f}ms)")

                # Seam delay between rows
                if row < ROWS:
                    next_row = row + 1
                    print(f"  >>> Seam: ID {row}8 -> {next_row}1  "
                          f"(wait {DELAY_SEAM*1000:.0f}ms)")
                    time.sleep(DELAY_SEAM)

            # ── Phase 1.2: Read Latch + LED OFF ─────────────────────────
            print(f"\n  Phase 1.2  ReadLatch -> LEDoff(1001-1008)")
            print(f"  {'='*84}")
            log_csv(csv_file, cycle, 0, '', '', '', '', 'PHASE_1.2_START', 'INFO')

            sc2 = 0
            for row in range(1, ROWS + 1):
                row_t0 = time.time()
                row_ok = 0

                print(f"\n  Row {row}  (ID {row}1 - {row}8)")
                print(f"  {'-'*84}")

                for col in range(1, COLS + 1):
                    sc2 += 1
                    _, _, uid, led = unit_plan[sc2 - 1]

                    print(f"  [{sc2:>3d}] ID:{uid:2d} L{led}", end='  ')

                    ok, _ = run_phase2(client, csv_file, cycle, sc2,
                                       row, col, uid, led)
                    if ok:
                        row_ok += 1
                        cycle_ok += 1
                        total_ok += 1
                    else:
                        cycle_fail += 1
                        total_fail += 1

                    if col < COLS:
                        time.sleep(DELAY_SUBCYCLE)

                row_ms = (time.time() - row_t0) * 1000
                print(f"  Row {row} done: {row_ok}/{COLS} OK  ({row_ms:.0f}ms)")

                # Seam delay between rows
                if row < ROWS:
                    next_row = row + 1
                    print(f"  >>> Seam: ID {row}8 -> {next_row}1  "
                          f"(wait {DELAY_SEAM*1000:.0f}ms)")
                    time.sleep(DELAY_SEAM)

            cycle_ms = (time.time() - cycle_t0) * 1000
            total_sub = total_ok + total_fail
            rate = total_ok / total_sub * 100 if total_sub > 0 else 0
            n_phases = ROWS * COLS * 2

            log_csv(csv_file, cycle, sc, '', '', '', '', 'CYCLE_END', 'INFO',
                    f"{cycle_ok}/{n_phases}", f"{cycle_ms:.0f}",
                    f"Overall:{total_ok}/{total_sub}")

            print(f"\n{'='*90}")
            print(f"CYCLE #{cycle} DONE  |  {cycle_ok}/{n_phases} OK  |  "
                  f"{cycle_ms/1000:.1f}s  |  Overall: {total_ok}/{total_sub} ({rate:.1f}%)")
            print(f"Next cycle in {DELAY_CYCLE/1000:.1f}s...")
            print(f"{'='*90}\n")
            time.sleep(DELAY_CYCLE)

    except KeyboardInterrupt:
        total_sub = total_ok + total_fail
        runtime = datetime.now() - program_start
        rate = total_ok / total_sub * 100 if total_sub > 0 else 0

        print(f"\n\n{'='*90}")
        print(f"  STOPPED BY USER")
        print(f"  Cycles       : {cycle}")
        print(f"  Sub-cycles   : {total_sub}")
        print(f"  Success      : {total_ok}")
        print(f"  Failed       : {total_fail}")
        print(f"  Success Rate : {rate:.1f}%")
        print(f"  Runtime      : {runtime}")
        print(f"{'='*90}")

        log_csv(csv_file, cycle, total_sub, '', '', '', '', 'STOP', 'INFO',
                f"{total_ok}/{total_sub}", '', f"Runtime:{runtime}")

    finally:
        client.close()
        log_csv(csv_file, cycle, 0, '', '', '', '', 'DISCONNECT', 'OK')

    print("Program terminated")


if __name__ == "__main__":
    main()
