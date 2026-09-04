"""Bench acceptance for the v3.5.0 window engine (type 10 / mask boards).

    python tools/window_engine_test.py --port COM5 --id 11
    python tools/window_engine_test.py --tcp 192.168.0.202 --id 11

Needs pymodbus; the LGS-Test-Tool venv has it:
    "../LGS-Test-Tool/.venv/Scripts/python.exe" tools/window_engine_test.py ...

What it proves, in order — each step read back from the wire, not assumed:

  1  All Off resets to a known state (reg 61 == 0, coils 1001-1008 all 0)
  2  windows accumulate: 1001, 1002, 1003 on -> reg 61 walks 0x01,0x03,0x07
     and ALL THREE enable coils read 1 — the assert that separates the
     window engine from radio, where the second coil would clear the first
  3  one window off leaves the siblings lit (0x05, coils 1/0/1)
  4  re-commanding a lit window disturbs nothing
  5  reg 11 = last window commanded on (while lit)
  6  max-on-time acts PER WINDOW: preset 2 clamped to 3 s goes out alone
  7  the 1022 combo lights its window without closing siblings (and the
     latch part resolves either way: pulse on a latch board, immediate
     reject-sync on a bare bench board)
  8  identify (509) blinks and then restores the lit set untouched
  9  All Off ends clean

DO NOT run this through the Queen-Sirikit gateway while a soak is running —
one RS485 master at a time. The desk USB-RS485 adapter is the safe path.
"""
from __future__ import annotations

import argparse
import sys
import time

try:
    from pymodbus.client import ModbusSerialClient, ModbusTcpClient
except ImportError:  # pragma: no cover
    print("pymodbus not installed — run with the LGS-Test-Tool venv python")
    sys.exit(2)

REG_ACTIVE = 11
REG_LIT = 61
REG_P2_MAX_ON = 124
COIL_ENABLE = 1000   # + n
COIL_LATCH_COMBO = 1020  # + n
COIL_IDENTIFY = 509
COIL_ALL_OFF = 511

passed = 0
failed = 0


def check(label: str, got, want) -> None:
    global passed, failed
    ok = got == want
    passed += ok
    failed += not ok
    print(f"  {'OK  ' if ok else 'FAIL'} {label}: got {got!r}"
          + ("" if ok else f", wanted {want!r}"))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", help="serial port of the desk RS485 adapter")
    ap.add_argument("--tcp", help="gateway IP (NOT while a soak runs)")
    ap.add_argument("--id", type=int, default=11)
    ap.add_argument("--baud", type=int, default=9600)
    a = ap.parse_args()
    if bool(a.port) == bool(a.tcp):
        ap.error("exactly one of --port / --tcp")

    if a.port:
        c = ModbusSerialClient(port=a.port, baudrate=a.baud, timeout=1.5)
    else:
        c = ModbusTcpClient(host=a.tcp, port=502, timeout=6.0)
    if not c.connect():
        print("cannot connect")
        return 1
    dev = {"device_id": a.id}

    def coil(addr: int, value: bool) -> None:
        r = c.write_coil(addr, value, **dev)
        if r.isError():
            raise RuntimeError(f"write coil {addr} failed: {r}")

    def coils(addr: int, n: int) -> list:
        r = c.read_coils(addr, count=n, **dev)
        if r.isError():
            raise RuntimeError(f"read coils {addr} failed: {r}")
        return [int(b) for b in r.bits[:n]]

    def reg(addr: int) -> int:
        r = c.read_holding_registers(addr, count=1, **dev)
        if r.isError():
            raise RuntimeError(f"read reg {addr} failed: {r}")
        return r.registers[0]

    def wreg(addr: int, value: int) -> None:
        r = c.write_register(addr, value, **dev)
        if r.isError():
            raise RuntimeError(f"write reg {addr} failed: {r}")

    def lit(timeout_s: float = 3.0, want: int | None = None) -> int:
        """reg 61, polling: the diagnostics publisher runs once a second."""
        deadline = time.monotonic() + timeout_s
        v = reg(REG_LIT)
        while want is not None and v != want and time.monotonic() < deadline:
            time.sleep(0.25)
            v = reg(REG_LIT)
        return v

    fw = reg(1)
    print(f"module id {a.id}, fw {fw}, device type {reg(0)}")
    if fw < 30500:
        print("  !! firmware older than v3.5.0 — this board has no window engine")

    print("\n[1] All Off -> clean slate")
    coil(COIL_ALL_OFF, True)
    check("reg 61", lit(want=0), 0)
    check("enable coils", coils(COIL_ENABLE + 1, 8), [0] * 8)

    print("\n[2] windows accumulate (the anti-radio assert)")
    for n, want in ((1, 0x01), (2, 0x03), (3, 0x07)):
        coil(COIL_ENABLE + n, True)
        check(f"reg 61 after window {n} on", lit(want=want), want)
    check("coils 1-3 all lit at once", coils(COIL_ENABLE + 1, 3), [1, 1, 1])

    print("\n[3] one off, siblings stay")
    coil(COIL_ENABLE + 2, False)
    check("reg 61", lit(want=0x05), 0x05)
    check("coils 1-3", coils(COIL_ENABLE + 1, 3), [1, 0, 1])

    print("\n[4] re-command a lit window: no disturbance")
    coil(COIL_ENABLE + 1, True)
    time.sleep(1.2)
    check("reg 61 unchanged", lit(), 0x05)

    print("\n[5] reg 11 = last commanded-on, while lit")
    check("reg 11", reg(REG_ACTIVE), 1)

    print("\n[6] max-on-time is per window")
    old = reg(REG_P2_MAX_ON)
    wreg(REG_P2_MAX_ON, 3)
    coil(COIL_ENABLE + 2, True)
    check("window 2 joined", lit(want=0x07), 0x07)
    time.sleep(4.5)
    check("window 2 timed out ALONE", lit(want=0x05), 0x05)
    check("coil 1002 cleared by timeout", coils(COIL_ENABLE + 2, 1), [0])
    wreg(REG_P2_MAX_ON, old)

    print("\n[7] latch combo lights its window, siblings untouched")
    coil(COIL_LATCH_COMBO + 2, True)
    check("reg 61 has windows 1,2,3", lit(want=0x07), 0x07)
    deadline = time.monotonic() + 4.0
    while coils(COIL_LATCH_COMBO + 2, 1)[0] and time.monotonic() < deadline:
        time.sleep(0.2)
    check("combo coil resolved", coils(COIL_LATCH_COMBO + 2, 1), [0])
    check("enable coil synced", coils(COIL_ENABLE + 2, 1), [1])

    print("\n[8] identify overlays then restores the set (watch the mask)")
    coil(COIL_IDENTIFY, True)
    time.sleep(6.0)
    check("reg 61 restored", lit(), 0x07)
    check("coils untouched", coils(COIL_ENABLE + 1, 3), [1, 1, 1])

    print("\n[9] All Off ends clean")
    coil(COIL_ALL_OFF, True)
    check("reg 61", lit(want=0), 0)

    c.close()
    print(f"\n{passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
