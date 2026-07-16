#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LGS Standard Module (R5.0) — Modbus RTU serial sweep tester
===========================================================
Connects to the board over a USB-to-RS485 adapter (a COM port) and exercises
the whole R5.0 Modbus address set defined in src/svc/modbus_map.h, logging every
transaction to a CSV under logs/. This is the on-hardware "golden-log" gate that
doc/ARCHITECTURE.md asks for.

Phases (run in order):
  1. CONNECT   - open the port, auto-discover the slave ID (or use --id)
  2. READ      - read every holding register (FC03) + coil state (FC01), decoded
  3. WRITE     - write-verify-restore the safe writable registers + state coils
  4. LED       - set a colour and toggle the ring (coil 1001), then restore
  5. LATCH     - fire the unlock coil 1020 / 1021 (PHYSICAL solenoid; gated)
  6. SUMMARY   - per-phase OK/FAIL/ERR counts; exit 0 only if no FAIL

Safety: the destructive coils (500/501/502 factory reset, 503 persist+reboot,
504 soft reset) and the identity registers 3/4 (baud/ID) are NOT touched — they
would reboot the board or drop it off the bus mid-sweep. The latch coils fire a
real solenoid and are gated behind a confirmation prompt (skip with --yes,
disable with --no-latch).

Requirements: pymodbus>=3.7, pyserial>=3.5  (installed in the conda base env).

Usage (Windows / conda base):
  & "$env:USERPROFILE\\miniconda3\\python.exe" tools/test_modbus_rtu.py --list-ports
  & "$env:USERPROFILE\\miniconda3\\python.exe" tools/test_modbus_rtu.py --port COM30
  & "$env:USERPROFILE\\miniconda3\\python.exe" tools/test_modbus_rtu.py --port COM30 --id 247 --no-latch
  & "$env:USERPROFILE\\miniconda3\\python.exe" tools/test_modbus_rtu.py --port COM30 --yes --loops 3
"""

import argparse
import csv
import logging
import os
import sys
import time
from datetime import datetime

try:
    from pymodbus.client import ModbusSerialClient
except ImportError:
    print("Error: pymodbus not installed.  Run:  pip install pymodbus pyserial")
    sys.exit(1)

try:
    from serial.tools import list_ports
except ImportError:
    list_ports = None

# Keep pymodbus from flooding the console; we do our own reporting.
logging.getLogger("pymodbus").setLevel(logging.WARNING)

# --------------------------------------------------------------------------- #
# R5.0 address map — mirrors src/svc/modbus_map.h (the firmware SSOT).
# Keep in lock-step with that header: one row here per address there.
# --------------------------------------------------------------------------- #

DEVICE_TYPES = {10: "STANDARD", 20: "NARCOTIC", 30: "LITE", 40: "DELIVERY"}
BAUD_WHITELIST = (9600, 19200, 38400, 57600)


def dec_plain(raw, unit=""):
    return f"{raw}{(' ' + unit) if unit else ''}"


def dec_temp(raw, unit=""):
    c = raw - 65536 if raw >= 32768 else raw   # signed int16
    return f"{c / 100.0:.2f} C  (raw {raw})"


def dec_device_type(raw, unit=""):
    return f"{raw} = {DEVICE_TYPES.get(raw, '?')}"


def dec_fw(raw, unit=""):
    s = str(raw).zfill(5)                       # ddmmy date code
    return f"{raw}  (dd/mm/y = {s[0:2]}/{s[2:4]}/*{s[4]})"


def dec_hw(raw, unit=""):
    return f"{raw}  (R{raw // 100}.{(raw // 10) % 10})"


def dec_baud(raw, unit=""):
    ok = "" if raw in BAUD_WHITELIST else "  (NOT in whitelist!)"
    return f"{raw} bps{ok}"


# Holding registers (FC03 read / FC06 write). decode = pretty-printer for READ.
REGISTERS = [
    # addr, name,                 unit,  decoder
    (0,   "Device Type",          "",    dec_device_type),
    (1,   "Firmware Version",     "",    dec_fw),
    (2,   "Hardware Version",     "",    dec_hw),
    (3,   "Baud Rate",            "bps", dec_baud),
    (4,   "Slave ID",             "",    dec_plain),
    (20,  "Room Temp",            "C",   dec_temp),
    (21,  "Board Temp",           "C",   dec_temp),
    (40,  "Time After Unlock",    "s",   dec_plain),
    (60,  "Display Number",       "",    dec_plain),
    (80,  "Unlock Delay",         "ms",  dec_plain),
    (110, "LED Brightness",       "%",   dec_plain),
    (111, "LED Red",              "",    dec_plain),
    (112, "LED Green",            "",    dec_plain),
    (113, "LED Blue",             "",    dec_plain),
    (114, "LED Max On-Time",      "s",   dec_plain),
    (190, "Global Brightness",    "%",   dec_plain),
    (194, "Global Max On-Time",   "s",   dec_plain),
    (200, "Total LED On Count",   "",    dec_plain),
    (201, "Total LED On Time",    "s",   dec_plain),
    (210, "LED 1 On Count",       "",    dec_plain),
    (211, "LED 1 On Time",        "s",   dec_plain),
]

# Coils (FC01 read / FC05 write). danger: excluded from every write path.
COILS = [
    # addr, name,                         danger
    (500,  "Factory Reset (arm)",         True),
    (501,  "Apply Reset (keep ID)",       True),
    (502,  "Apply Reset (all data)",      True),
    (503,  "Write to EEPROM (+reboot)",   True),
    (504,  "Software Reset",              True),
    (1001, "LED 1 Enable",                False),
    (1010, "Display Enable (stub)",       False),
    (1011, "LED 1 + Display (stub)",      False),
    (1020, "Latch Trigger",               False),  # gated separately (physical)
    (1021, "LED 1 + Latch",               False),  # gated separately (physical)
]

# Safe register write tests: (addr, name, test_value, verify_addr).
# verify_addr differs from addr for the fan-out registers 190->110 and 194->114:
# writing them copies into the target register, so we check the target's value.
WRITE_TESTS = [
    (60,  "Display Number",     1234, 60),
    (80,  "Unlock Delay",        250, 80),
    (110, "LED Brightness",       50, 110),
    (111, "LED Red",              11, 111),
    (112, "LED Green",            22, 112),
    (113, "LED Blue",             33, 113),
    (114, "LED Max On-Time",    1800, 114),
    (190, "Global Brightness",    45, 110),   # fan-out -> reg 110
    (194, "Global Max On-Time", 2400, 114),   # fan-out -> reg 114
]

# State coils that hold their value (safe to toggle + restore).
STATE_COILS = [
    (1001, "LED 1 Enable"),
    (1010, "Display Enable (stub)"),
    (1011, "LED 1 + Display (stub)"),
]

LATCH_COOLDOWN_S = 2.2   # firmware enforces >=2000 ms between unlock pulses
INTER_TXN_S = 0.025      # small RS485 breather between transactions

# --------------------------------------------------------------------------- #
# Counters + CSV logging
# --------------------------------------------------------------------------- #

class Stats:
    def __init__(self):
        self.ok = 0
        self.fail = 0
        self.err = 0

    def add(self, result):
        if result == "OK":
            self.ok += 1
        elif result == "FAIL":
            self.fail += 1
        else:
            self.err += 1


def open_csv(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fh = open(path, "w", newline="", encoding="utf-8")
    w = csv.writer(fh)
    w.writerow(["timestamp", "loop", "phase", "fc", "addr", "name",
                "op", "raw", "decoded", "expected", "result", "latency_ms", "note"])
    return fh, w


def log_row(writer, loop, phase, fc, addr, name, op, raw, decoded,
            expected, result, latency_ms, note=""):
    writer.writerow([datetime.now().isoformat(timespec="milliseconds"), loop,
                     phase, fc, addr, name, op, raw, decoded, expected, result,
                     f"{latency_ms:.1f}", note])


# --------------------------------------------------------------------------- #
# Transaction wrappers — always return (ok, value, latency_ms, note)
# --------------------------------------------------------------------------- #

def _timed(fn):
    t = time.time()
    try:
        rsp = fn()
    except Exception as exc:                      # noqa: BLE001 - report anything
        return None, (time.time() - t) * 1000.0, f"EXC {type(exc).__name__}: {exc}"
    dt = (time.time() - t) * 1000.0
    if rsp is None or rsp.isError():
        return None, dt, f"ERR {rsp}"
    return rsp, dt, ""


def read_reg(client, addr, unit):
    rsp, dt, note = _timed(lambda: client.read_holding_registers(addr, count=1, device_id=unit))
    if rsp is None:
        return False, None, dt, note
    return True, rsp.registers[0], dt, ""


def read_coil(client, addr, unit):
    rsp, dt, note = _timed(lambda: client.read_coils(addr, count=1, device_id=unit))
    if rsp is None:
        return False, None, dt, note
    return True, 1 if rsp.bits[0] else 0, dt, ""


def write_reg(client, addr, value, unit):
    rsp, dt, note = _timed(lambda: client.write_register(addr, value, device_id=unit))
    return (rsp is not None), dt, note


def write_coil(client, addr, value, unit):
    rsp, dt, note = _timed(lambda: client.write_coil(addr, bool(value), device_id=unit))
    return (rsp is not None), dt, note


# --------------------------------------------------------------------------- #
# Discovery
# --------------------------------------------------------------------------- #

def list_serial_ports():
    if list_ports is None:
        print("  (pyserial not available — cannot list ports)")
        return
    ports = list(list_ports.comports())
    if not ports:
        print("  (no serial ports found)")
        return
    for p in ports:
        print(f"  {p.device:<8} - {p.description}")


def discover_id(port, baud, candidates, scan_timeout=0.25):
    """Probe reg 0 (device type) across candidate IDs; return the first responder."""
    scan = ModbusSerialClient(port=port, baudrate=baud, bytesize=8,
                              parity="N", stopbits=1, timeout=scan_timeout)
    if not scan.connect():
        print(f"  [ERR] cannot open {port} @ {baud} for scan")
        return None
    print(f"  scanning IDs on {port} @ {baud} 8N1 (probe reg 0)...")
    found = None
    try:
        for i, uid in enumerate(candidates):
            ok, val, _dt, _note = read_reg(scan, 0, uid)
            if ok:
                dtype = DEVICE_TYPES.get(val, "?")
                print(f"  [OK] ID {uid} responded  (Device Type {val} = {dtype})")
                found = uid
                break
            if i and i % 25 == 0:
                print(f"  ...probed {i}/{len(candidates)}")
    finally:
        scan.close()
    return found


# --------------------------------------------------------------------------- #
# Phases
# --------------------------------------------------------------------------- #

def banner(title):
    print("\n" + "=" * 72)
    print(f"  {title}")
    print("=" * 72)


def phase_read(client, unit, loop, writer, stats):
    banner("PHASE 2 — READ SWEEP (holding registers + coil states)")
    print(f"  {'Addr':>5}  {'Name':<22} {'Value (decoded)':<34} {'ms':>6}")
    print("  " + "-" * 70)
    for addr, name, unit_s, decoder in REGISTERS:
        ok, val, dt, note = read_reg(client, addr, unit)
        if ok:
            decoded = decoder(val, unit_s)
            print(f"  {addr:>5}  {name:<22} {decoded:<34} {dt:>6.1f}  [OK]")
            log_row(writer, loop, "READ", 3, addr, name, "read", val, decoded, "", "OK", dt)
            stats.add("OK")
        else:
            print(f"  {addr:>5}  {name:<22} {'<no reply>':<34} {dt:>6.1f}  [ERR] {note}")
            log_row(writer, loop, "READ", 3, addr, name, "read", "", "", "", "ERR", dt, note)
            stats.add("ERR")
        time.sleep(INTER_TXN_S)

    print()
    for addr, name, _danger in COILS:
        ok, val, dt, note = read_coil(client, addr, unit)
        if ok:
            print(f"  {addr:>5}  {name:<28} state={val}   {dt:>6.1f}  [OK]")
            log_row(writer, loop, "READ", 1, addr, name, "read", val, f"coil={val}", "", "OK", dt)
            stats.add("OK")
        else:
            print(f"  {addr:>5}  {name:<28} <no reply>  {dt:>6.1f}  [ERR] {note}")
            log_row(writer, loop, "READ", 1, addr, name, "read", "", "", "", "ERR", dt, note)
            stats.add("ERR")
        time.sleep(INTER_TXN_S)


def _read_reg_val(client, addr, unit):
    ok, val, _dt, _note = read_reg(client, addr, unit)
    return val if ok else None


def phase_write(client, unit, loop, writer, stats):
    banner("PHASE 3 — WRITE / VERIFY / RESTORE (safe registers + state coils)")

    # Snapshot every target register up-front so restores are exact even for the
    # fan-out pairs (190->110, 194->114) that share a target.
    targets = sorted({vaddr for _, _, _, vaddr in WRITE_TESTS} |
                     {addr for addr, _, _, _ in WRITE_TESTS if addr not in (190, 194)})
    original = {a: _read_reg_val(client, a, unit) for a in targets}

    for addr, name, test_val, vaddr in WRITE_TESTS:
        wok, dt, note = write_reg(client, addr, test_val, unit)
        if not wok:
            print(f"  {addr:>5}  {name:<22} write {test_val:<6} [ERR] {note}")
            log_row(writer, loop, "WRITE", 6, addr, name, "write", test_val, "", test_val, "ERR", dt, note)
            stats.add("ERR")
            time.sleep(INTER_TXN_S)
            continue
        time.sleep(INTER_TXN_S)
        rok, rb, dt2, note2 = read_reg(client, vaddr, unit)
        if not rok:
            print(f"  {addr:>5}  {name:<22} write {test_val:<6} readback [ERR] {note2}")
            log_row(writer, loop, "WRITE", 3, vaddr, name, "verify", "", "", test_val, "ERR", dt2, note2)
            stats.add("ERR")
        elif rb == test_val:
            tgt = "" if vaddr == addr else f" -> reg {vaddr}"
            print(f"  {addr:>5}  {name:<22} write {test_val:<6}{tgt} readback={rb}  [OK]")
            log_row(writer, loop, "WRITE", 3, vaddr, name, "verify", rb, f"echo{tgt}", test_val, "OK", dt2)
            stats.add("OK")
        else:
            print(f"  {addr:>5}  {name:<22} write {test_val:<6} readback={rb}  [FAIL] expected {test_val}")
            log_row(writer, loop, "WRITE", 3, vaddr, name, "verify", rb, "mismatch", test_val, "FAIL", dt2)
            stats.add("FAIL")
        time.sleep(INTER_TXN_S)

    # Restore originals (targets last-writer-wins is fine; we restore all).
    for a in targets:
        if original[a] is not None:
            write_reg(client, a, original[a], unit)
            time.sleep(INTER_TXN_S)
    print(f"  restored registers: {', '.join(str(a) for a in targets)}")

    # State coils: toggle to a known value, verify, restore.
    print()
    for addr, name in STATE_COILS:
        ok0, orig, _dt, _n = read_coil(client, addr, unit)
        target = 0 if (ok0 and orig == 1) else 1
        wok, dt, note = write_coil(client, addr, target, unit)
        time.sleep(INTER_TXN_S)
        rok, rb, dt2, note2 = read_coil(client, addr, unit)
        if wok and rok and rb == target:
            print(f"  {addr:>5}  {name:<28} set {target} readback={rb}  [OK]")
            log_row(writer, loop, "WRITE", 1, addr, name, "coil-verify", rb, f"coil={rb}", target, "OK", dt2)
            stats.add("OK")
        elif not wok:
            print(f"  {addr:>5}  {name:<28} set {target}  [ERR] {note}")
            log_row(writer, loop, "WRITE", 5, addr, name, "coil", target, "", target, "ERR", dt, note)
            stats.add("ERR")
        else:
            print(f"  {addr:>5}  {name:<28} set {target} readback={rb}  [FAIL]")
            log_row(writer, loop, "WRITE", 1, addr, name, "coil-verify", rb, "mismatch", target, "FAIL", dt2, note2)
            stats.add("FAIL")
        # restore
        if ok0:
            write_coil(client, addr, orig, unit)
            time.sleep(INTER_TXN_S)


def phase_led(client, unit, loop, writer, stats):
    banner("PHASE 4 — LED ACTUATION (visible: blue ring for ~1.5 s)")
    orig = {a: _read_reg_val(client, a, unit) for a in (110, 111, 112, 113)}
    _ok, orig_en, _dt, _n = read_coil(client, 1001, unit)

    # Clean enable edge: force off, set colour, then on.
    write_coil(client, 1001, 0, unit); time.sleep(INTER_TXN_S)
    write_reg(client, 110, 60, unit);  time.sleep(INTER_TXN_S)   # brightness
    write_reg(client, 111, 0, unit);   time.sleep(INTER_TXN_S)   # R
    write_reg(client, 112, 0, unit);   time.sleep(INTER_TXN_S)   # G
    write_reg(client, 113, 255, unit); time.sleep(INTER_TXN_S)   # B -> blue

    wok, dt, note = write_coil(client, 1001, 1, unit)
    time.sleep(INTER_TXN_S)
    rok, rb, dt2, _n2 = read_coil(client, 1001, unit)
    if wok and rok and rb == 1:
        print(f"  coil 1001 -> ON, readback={rb}  [OK]  (ring should be BLUE now)")
        log_row(writer, loop, "LED", 1, 1001, "LED 1 Enable", "on", rb, "ring blue", 1, "OK", dt2)
        stats.add("OK")
    else:
        print(f"  coil 1001 -> ON, readback={rb}  [FAIL/ERR] {note}")
        log_row(writer, loop, "LED", 1, 1001, "LED 1 Enable", "on", rb, "", 1, "FAIL", dt2, note)
        stats.add("FAIL")

    time.sleep(1.5)   # keep it visible

    write_coil(client, 1001, 0, unit); time.sleep(INTER_TXN_S)
    for a, v in orig.items():
        if v is not None:
            write_reg(client, a, v, unit); time.sleep(INTER_TXN_S)
    if orig_en is not None:
        write_coil(client, 1001, orig_en, unit); time.sleep(INTER_TXN_S)
    print("  ring off; LED config restored")


def _fire_latch(client, unit, coil, name, loop, writer, stats):
    t40_before = _read_reg_val(client, 40, unit)
    wok, dt, note = write_coil(client, coil, 1, unit)
    if not wok:
        print(f"  coil {coil} ({name}) write [ERR] {note}")
        log_row(writer, loop, "LATCH", 5, coil, name, "fire", 1, "", "", "ERR", dt, note)
        stats.add("ERR")
        return
    # Poll for the firmware to self-clear the command coil (pulse resolved).
    t0 = time.time()
    cleared_ms = None
    last = 1
    while time.time() - t0 < 1.5:
        ok, val, _d, _n = read_coil(client, coil, unit)
        if ok:
            last = val
            if val == 0:
                cleared_ms = (time.time() - t0) * 1000.0
                break
        time.sleep(0.02)
    t40_after = _read_reg_val(client, 40, unit)
    if cleared_ms is not None:
        hint = "pulsed (sense locked)" if cleared_ms > 150 else "accepted; no pulse (sense not locked / no latch wired)"
        print(f"  coil {coil} ({name}) fired; self-cleared in {cleared_ms:.0f} ms — {hint}")
        print(f"       reg40 before={t40_before}  after={t40_after}")
        log_row(writer, loop, "LATCH", 1, coil, name, "fire", 0,
                f"cleared {cleared_ms:.0f}ms; {hint}", "self-clear", "OK", dt,
                f"reg40 {t40_before}->{t40_after}")
        stats.add("OK")
    else:
        print(f"  coil {coil} ({name}) did NOT self-clear within 1.5 s (still {last})  [FAIL]")
        log_row(writer, loop, "LATCH", 1, coil, name, "fire", last, "no self-clear", "self-clear", "FAIL", dt)
        stats.add("FAIL")


def phase_latch(client, unit, loop, writer, stats, fires, test_1021):
    banner("PHASE 5 — LATCH ACTUATION (coil 1020 / 1021, PHYSICAL solenoid)")
    for n in range(fires):
        print(f"  --- fire {n + 1}/{fires} : coil 1020 (Latch Trigger) ---")
        _fire_latch(client, unit, 1020, "Latch Trigger", loop, writer, stats)
        if n < fires - 1 or test_1021:
            print(f"  cooldown {LATCH_COOLDOWN_S:.1f} s (firmware min interval)...")
            time.sleep(LATCH_COOLDOWN_S)
    if test_1021:
        print("  --- coil 1021 (LED 1 + Latch) ---")
        _fire_latch(client, unit, 1021, "LED 1 + Latch", loop, writer, stats)
        # 1021 leaves the ring on; turn it back off.
        time.sleep(0.2)
        write_coil(client, 1001, 0, unit)


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #

def main():
    ap = argparse.ArgumentParser(
        description="LGS R5.0 Modbus RTU serial sweep tester (read + write + LED + latch).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    ap.add_argument("-p", "--port", default="COM30", help="serial port of the USB-RS485 adapter")
    ap.add_argument("-b", "--baud", type=int, default=9600, choices=BAUD_WHITELIST, help="baud rate")
    ap.add_argument("-i", "--id", type=int, default=None,
                    help="slave ID (default: auto-discover by probing reg 0)")
    ap.add_argument("--loops", type=int, default=1, help="repeat the whole sweep this many times")
    ap.add_argument("--latch-fires", type=int, default=1, help="number of coil-1020 pulses per loop")
    ap.add_argument("--test-1021", action="store_true", help="also fire coil 1021 (LED + latch)")
    ap.add_argument("--no-latch", action="store_true", help="skip the physical latch phase entirely")
    ap.add_argument("--no-led", action="store_true", help="skip the visible LED phase")
    ap.add_argument("-y", "--yes", action="store_true", help="skip the latch confirmation prompt")
    ap.add_argument("--csv", default=None, help="CSV log path (default: logs/rtu_sweep_<ts>.csv)")
    ap.add_argument("--timeout", type=float, default=1.0, help="RTU response timeout (s)")
    ap.add_argument("--list-ports", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()

    if args.list_ports:
        print("Available serial ports:")
        list_serial_ports()
        return 0

    # ----- Phase 1: connect + discover -------------------------------------- #
    banner("PHASE 1 — CONNECT")
    print(f"  port={args.port}  baud={args.baud}  framing=8N1  timeout={args.timeout}s")

    unit = args.id
    if unit is None:
        candidates = [247] + [i for i in range(1, 247)]   # factory default first
        unit = discover_id(args.port, args.baud, candidates)
        if unit is None:
            print("  [ERR] no device answered on any ID 1..247.")
            print("        Check wiring / power / baud, or pass --id explicitly.")
            return 2
    print(f"  using slave ID {unit}")

    client = ModbusSerialClient(port=args.port, baudrate=args.baud, bytesize=8,
                                parity="N", stopbits=1, timeout=args.timeout)
    if not client.connect():
        print(f"  [ERR] could not open {args.port}")
        return 2

    # Confirm the physical latch phase before doing anything.
    do_latch = not args.no_latch
    if do_latch and not args.yes:
        total = args.latch_fires * args.loops + (args.loops if args.test_1021 else 0)
        try:
            ans = input(f"\n  Phase 5 will FIRE THE SOLENOID ~{total} time(s). Continue? [y/N] ")
        except EOFError:
            ans = ""
        if ans.strip().lower() not in ("y", "yes"):
            print("  latch phase disabled (answer was not 'y').")
            do_latch = False

    csv_path = args.csv or os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        "logs", f"rtu_sweep_{datetime.now().strftime('%Y-%m-%d_%H%M%S')}.csv")
    fh, writer = open_csv(csv_path)
    stats = Stats()

    try:
        for loop in range(1, args.loops + 1):
            if args.loops > 1:
                banner(f"LOOP {loop}/{args.loops}")
            phase_read(client, unit, loop, writer, stats)
            phase_write(client, unit, loop, writer, stats)
            if not args.no_led:
                phase_led(client, unit, loop, writer, stats)
            if do_latch:
                phase_latch(client, unit, loop, writer, stats, args.latch_fires, args.test_1021)
    except KeyboardInterrupt:
        print("\n  interrupted — driving latch/LED off before exit...")
        try:
            write_coil(client, 1020, 0, unit)
            write_coil(client, 1001, 0, unit)
        except Exception:
            pass
    finally:
        client.close()
        fh.close()

    banner("SUMMARY")
    total = stats.ok + stats.fail + stats.err
    print(f"  transactions: {total}   OK={stats.ok}   FAIL={stats.fail}   ERR={stats.err}")
    print(f"  CSV log: {csv_path}")
    if stats.fail == 0 and stats.err == 0:
        print("  RESULT: PASS")
        return 0
    print("  RESULT: check FAIL/ERR rows above")
    return 1


if __name__ == "__main__":
    sys.exit(main())
