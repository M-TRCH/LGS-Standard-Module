"""Ring regression: prove v3.5.0 did NOT change a type-20 board.

    python tools/ring_regression_test.py --port COM8 --id 247

The window engine (v3.5.0) shares app/led_control.cpp with the ring engine.
The isolation claim is that a ring board executes exactly the statements it
executed on v3.4.0, because every handler branches to the window path before
its first radio statement. That claim is worth nothing unless a ring board
is made to demonstrate radio behaviour on the new firmware.

So this asserts the OPPOSITE of window_engine_test.py at every point. Where
that one proves three coils can read 1 at once, this proves that lighting a
second preset clears the first -- and it is the same assertion, checked on
the same firmware, distinguished only by what the board is.

Steps:
  1  All Off -> clean slate
  2  preset 1 on -> reg 11 = 1, reg 61 = 0x01, coil 1001 = 1
  3  RADIO: preset 2 on while 1 is lit -> coil 1001 auto-clears, only
     preset 2 remains. This is the whole point of the file.
  4  writing 0 to a NON-active preset's coil changes nothing
  5  writing 0 to the ACTIVE preset's coil goes dark
  6  latch combo 1022 -> radio switch, combo self-clears, enable syncs
  7  display combo 1013 -> preset 3 + display coil 1010 set
  8  reg 60 renders on the OLED (visual: the number appears)
  9  identify blinks white and hands the ring back to its preset
 10  max-on-time expires the single active preset
 11  health bits: a ring board has its OLED (bit1) -- the type-10 tell
 12  All Off ends clean

Run it on a board whose reg 0 reports 20. It refuses to run on a type 10,
where every one of these expectations is wrong by design.
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

REG_DEVICE_TYPE = 0
REG_HEALTH = 9
REG_ACTIVE = 11
REG_NUM_DISPLAY = 60
REG_LIT = 61
COIL_ENABLE = 1000
COIL_DISPLAY_ENABLE = 1010
COIL_DISPLAY_COMBO = 1010      # + n
COIL_LATCH_COMBO = 1020        # + n
COIL_IDENTIFY = 509
COIL_ALL_OFF = 511

passed = 0
failed = 0


def check(label, got, want):
    global passed, failed
    ok = got == want
    passed += ok
    failed += not ok
    print(f"  {'OK  ' if ok else 'FAIL'} {label}: got {got!r}"
          + ("" if ok else f", wanted {want!r}"))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port")
    ap.add_argument("--tcp")
    ap.add_argument("--id", type=int, default=247)
    ap.add_argument("--baud", type=int, default=9600)
    a = ap.parse_args()
    if bool(a.port) == bool(a.tcp):
        ap.error("exactly one of --port / --tcp")

    c = (ModbusSerialClient(port=a.port, baudrate=a.baud, timeout=1.5)
         if a.port else ModbusTcpClient(host=a.tcp, port=502, timeout=6.0))
    if not c.connect():
        print("cannot connect")
        return 1
    dev = {"device_id": a.id}

    def coil(addr, value):
        r = c.write_coil(addr, value, **dev)
        if r.isError():
            raise RuntimeError(f"write coil {addr}: {r}")

    def coils(addr, n):
        r = c.read_coils(addr, count=n, **dev)
        if r.isError():
            raise RuntimeError(f"read coils {addr}: {r}")
        return [int(b) for b in r.bits[:n]]

    def reg(addr):
        r = c.read_holding_registers(addr, count=1, **dev)
        if r.isError():
            raise RuntimeError(f"read reg {addr}: {r}")
        return r.registers[0]

    def wreg(addr, value):
        r = c.write_register(addr, value, **dev)
        if r.isError():
            raise RuntimeError(f"write reg {addr}: {r}")

    def settle(addr, want, timeout_s=3.0):
        """Registers 11 and 61 are published once a second by diag_control."""
        deadline = time.monotonic() + timeout_s
        v = reg(addr)
        while v != want and time.monotonic() < deadline:
            time.sleep(0.25)
            v = reg(addr)
        return v

    dtype, fw = reg(REG_DEVICE_TYPE), reg(1)
    print(f"module id {a.id}, device type {dtype}, fw {fw}")
    if dtype != 20:
        print(f"  !! this board reports type {dtype}, not 20 — every assertion "
              f"below is written for a RING board. Refusing to run.")
        c.close()
        return 2
    if fw < 30500:
        print(f"  !! fw {fw} predates the window engine; this proves nothing "
              f"about the change being isolated. Flash v3.5.0 first.")
        c.close()
        return 2

    print("\n[1] All Off -> clean slate")
    coil(COIL_ALL_OFF, True)
    check("reg 11", settle(REG_ACTIVE, 0), 0)
    check("reg 61", settle(REG_LIT, 0), 0)
    check("enable coils", coils(COIL_ENABLE + 1, 8), [0] * 8)

    print("\n[2] preset 1 on")
    coil(COIL_ENABLE + 1, True)
    check("reg 11 = 1", settle(REG_ACTIVE, 1), 1)
    check("reg 61 = 0x01 (one bit, mirroring the active preset)",
          settle(REG_LIT, 0x01), 0x01)
    check("coil 1001 set", coils(COIL_ENABLE + 1, 1), [1])

    print("\n[3] RADIO — preset 2 while 1 is lit CLOSES preset 1")
    coil(COIL_ENABLE + 2, True)
    check("reg 11 = 2", settle(REG_ACTIVE, 2), 2)
    check("reg 61 = 0x02 ONLY (window engine would read 0x03)",
          settle(REG_LIT, 0x02), 0x02)
    check("coil 1001 auto-cleared", coils(COIL_ENABLE + 1, 1), [0])
    check("coil 1002 set", coils(COIL_ENABLE + 2, 1), [1])
    check("exactly one enable coil reads 1",
          sum(coils(COIL_ENABLE + 1, 8)), 1)

    print("\n[4] writing 0 to a NON-active preset does nothing")
    coil(COIL_ENABLE + 5, False)
    time.sleep(1.4)
    check("preset 2 still active", reg(REG_ACTIVE), 2)
    check("reg 61 unchanged", reg(REG_LIT), 0x02)

    print("\n[5] writing 0 to the ACTIVE preset goes dark")
    coil(COIL_ENABLE + 2, False)
    check("reg 11 = 0", settle(REG_ACTIVE, 0), 0)
    check("reg 61 = 0", settle(REG_LIT, 0), 0)

    print("\n[6] latch combo 1022 radio-switches like the plain coil")
    coil(COIL_ENABLE + 4, True)
    settle(REG_ACTIVE, 4)
    coil(COIL_LATCH_COMBO + 2, True)
    check("reg 11 switched to 2", settle(REG_ACTIVE, 2), 2)
    check("preset 4 was closed", settle(REG_LIT, 0x02), 0x02)
    deadline = time.monotonic() + 4.0
    while coils(COIL_LATCH_COMBO + 2, 1)[0] and time.monotonic() < deadline:
        time.sleep(0.2)
    check("combo coil self-cleared", coils(COIL_LATCH_COMBO + 2, 1), [0])
    check("enable coil synced", coils(COIL_ENABLE + 2, 1), [1])

    print("\n[7] display combo 1013 lights preset 3 AND the display")
    coil(COIL_DISPLAY_COMBO + 3, True)
    check("reg 11 = 3", settle(REG_ACTIVE, 3), 3)
    check("preset 2 closed by radio", settle(REG_LIT, 0x04), 0x04)
    check("display enable coil 1010 set", coils(COIL_DISPLAY_ENABLE, 1), [1])

    print("\n[8] reg 60 renders on the OLED  (LOOK AT THE SCREEN)")
    for n in (7, 42, 500, 999):
        wreg(REG_NUM_DISPLAY, n)
        check(f"reg 60 reflects {n}", reg(REG_NUM_DISPLAY), n)
        time.sleep(1.2)
    wreg(REG_NUM_DISPLAY, 1234)
    check("out-of-range clamps to 999", reg(REG_NUM_DISPLAY), 999)
    time.sleep(1.5)

    print("\n[9] identify blinks white, then hands the ring back")
    coil(COIL_IDENTIFY, True)
    time.sleep(6.5)
    check("preset 3 restored", reg(REG_ACTIVE), 3)
    check("reg 61 restored", reg(REG_LIT), 0x04)

    print("\n[10] max-on-time expires the active preset")
    old = reg(134)                      # preset 3 max-on-time
    coil(COIL_ALL_OFF, True)
    settle(REG_LIT, 0)
    wreg(134, 3)
    coil(COIL_ENABLE + 3, True)
    check("preset 3 lit", settle(REG_LIT, 0x04), 0x04)
    time.sleep(5.0)
    check("expired on its own", settle(REG_LIT, 0), 0)
    check("its enable coil cleared", coils(COIL_ENABLE + 3, 1), [0])
    wreg(134, old)

    print("\n[11] a ring board carries the parts a type 10 does not")
    h = reg(REG_HEALTH)
    print(f"       health bits = 0b{h:05b}"
          f"  (bit0 AT24, bit1 OLED, bit2 room, bit3 board, bit4 latch)")
    check("bit0 AT24 ok", (h >> 0) & 1, 1)
    check("bit1 OLED present — the difference from a type-10 board",
          (h >> 1) & 1, 1)

    print("\n[12] All Off ends clean")
    coil(COIL_ALL_OFF, True)
    check("reg 61", settle(REG_LIT, 0), 0)
    check("reg 11", settle(REG_ACTIVE, 0), 0)
    check("display coil cleared", coils(COIL_DISPLAY_ENABLE, 1), [0])

    c.close()
    print(f"\n{passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
