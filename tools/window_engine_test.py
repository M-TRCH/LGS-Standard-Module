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
    ap.add_argument("--deep", action="store_true",
                    help="also run steps 10-15: all eight windows, per-window "
                         "statistics, persistence across a reboot, global "
                         "fan-out, unlimited max-on, and a churn stress. "
                         "Slower (~2 min) and reboots the module.")
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

    print("\n[4] writing 1 to an ALREADY-lit window's coil is a no-op")
    # The enable coils are CHANGE watches, so a write that does not change
    # the coil never reaches the handler: nothing repaints and winLastCmd
    # does not move. Same semantics the ring engine has always had -- and
    # the reason an earlier version of this test wrongly expected reg 11
    # to follow a re-command.
    coil(COIL_ENABLE + 1, True)
    time.sleep(1.4)
    check("reg 61 unchanged", lit(), 0x05)
    check("reg 11 unchanged", reg(REG_ACTIVE), 3)

    print("\n[5] reg 11 = last window actually commanded on, else lowest lit")
    check("reg 11 = 3 (last real command, still lit)", reg(REG_ACTIVE), 3)
    coil(COIL_ENABLE + 3, False)          # window 1 is now the only one lit
    check("reg 61", lit(want=0x01), 0x01)
    time.sleep(1.4)
    check("reg 11 falls back to the lowest lit", reg(REG_ACTIVE), 1)
    coil(COIL_ENABLE + 3, True)           # restore the pair for later steps
    check("reg 61 back to windows 1+3", lit(want=0x05), 0x05)

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

    if a.deep:
        deep(coil, coils, reg, wreg, lit, dev, c)

    c.close()
    print(f"\n{passed} passed, {failed} failed")
    return 1 if failed else 0


def deep(coil, coils, reg, wreg, lit, dev, client):
    """Steps 10-15: the parts of the window engine nothing else exercises."""
    import random

    ON_CNT = lambda n: 200 + 10 * n        # per-window lifetime on-count
    ON_TIME = lambda n: 201 + 10 * n       # per-window lifetime seconds
    BASE = lambda n: 100 + 10 * n          # +0 bright +1 R +2 G +3 B +4 max-on

    print("\n[10] all EIGHT windows lit at once (the full product claim)")
    coil(COIL_ALL_OFF, True)
    lit(want=0)
    for n in range(1, 9):
        coil(COIL_ENABLE + n, True)
        time.sleep(0.2)
    check("reg 61 = 0xFF", lit(want=0xFF), 0xFF)
    check("all eight enable coils read 1", coils(COIL_ENABLE + 1, 8), [1] * 8)
    check("reg 11 = 8 (last commanded)", reg(REG_ACTIVE), 8)
    coil(COIL_ALL_OFF, True)
    check("All Off clears all eight", lit(want=0), 0)

    print("\n[11] statistics accumulate PER WINDOW, not into a shared counter")
    coil(510, True)                        # clear statistics
    time.sleep(2.0)
    check("window 5 count starts at 0", reg(ON_CNT(5)), 0)
    check("window 2 count starts at 0", reg(ON_CNT(2)), 0)
    coil(COIL_ENABLE + 5, True)
    time.sleep(4.0)
    coil(COIL_ENABLE + 5, False)
    coil(COIL_ENABLE + 2, True)
    time.sleep(2.0)
    coil(COIL_ENABLE + 2, False)
    time.sleep(2.0)                        # let statsPublishRegisters catch up
    c5, t5 = reg(ON_CNT(5)), reg(ON_TIME(5))
    c2, t2 = reg(ON_CNT(2)), reg(ON_TIME(2))
    check("window 5 on-count", c5, 1)
    check("window 2 on-count", c2, 1)
    print(f"       window 5 ran {t5} s (expected ~4), window 2 ran {t2} s (expected ~2)")
    check("window 5 runtime is the longer of the two", t5 > t2, True)
    check("window 1 untouched", reg(ON_CNT(1)), 0)
    check("window 8 untouched", reg(ON_CNT(8)), 0)
    check("total on-count = 2", reg(200), 2)

    print("\n[12] those statistics survive a reboot (AT24 fold on the way out)")
    boots_before = reg(7)
    coil(504, True)                        # software reset
    print("       module resetting ...")
    time.sleep(3.0)
    deadline = time.monotonic() + 25
    while time.monotonic() < deadline:
        try:
            r = client.read_holding_registers(1, count=1, **dev)
            if not r.isError():
                break
        except Exception:
            pass
        time.sleep(1.0)
    time.sleep(2.5)                        # let the publisher run once
    check("module came back", reg(1), 30500)
    check("boot counter advanced", reg(7), boots_before + 1)
    check("window 5 count survived", reg(ON_CNT(5)), 1)
    check("window 2 count survived", reg(ON_CNT(2)), 1)
    check("window 5 runtime survived", reg(ON_TIME(5)) >= t5, True)
    check("reg 61 is 0 after a reboot", lit(want=0), 0)

    print("\n[13] global fan-out reaches every window's own registers")
    wreg(194, 7)                           # global max-on-time
    time.sleep(0.5)
    check("every max-on became 7", [reg(BASE(n) + 4) for n in range(1, 9)], [7] * 8)
    wreg(190, 55)                          # global brightness
    time.sleep(0.5)
    check("every brightness became 55", [reg(BASE(n) + 0) for n in range(1, 9)], [55] * 8)
    wreg(190, 150)                         # out of range -> clamps and reflects
    time.sleep(0.5)
    check("out-of-range brightness clamped to 100", reg(190), 100)
    wreg(190, 80)
    time.sleep(0.5)

    print("\n[14] max-on-time 0 means unlimited, per window")
    wreg(BASE(3) + 4, 0)                   # window 3 unlimited
    wreg(BASE(4) + 4, 3)                   # window 4 expires in 3 s
    time.sleep(0.5)
    coil(COIL_ENABLE + 3, True)
    coil(COIL_ENABLE + 4, True)
    check("both lit", lit(want=0x0C), 0x0C)
    time.sleep(6.0)
    check("window 4 expired, window 3 still lit", lit(want=0x04), 0x04)
    coil(COIL_ALL_OFF, True)
    lit(want=0)
    wreg(194, 3600)                        # restore the shipping default
    time.sleep(0.5)

    print("\n[16] display combos 1011-1018 keep window semantics on a mask board")
    # onLedDisplayChange has its own useMask branch and nothing else in this
    # file reaches it. A mask board has no OLED, so the display half is a
    # no-op in hardware -- the window half still has to behave.
    coil(COIL_ALL_OFF, True)
    lit(want=0)
    coil(1010 + 3, True)                   # window 3 + display
    check("window 3 lit via its display combo", lit(want=0x04), 0x04)
    check("its enable coil mirrored", coils(COIL_ENABLE + 3, 1), [1])
    coil(1010 + 5, True)                   # window 5 too -- must not close 3
    check("window 5 joined, 3 stayed", lit(want=0x14), 0x14)
    check("both enable coils set", [coils(COIL_ENABLE + 3, 1)[0],
                                    coils(COIL_ENABLE + 5, 1)[0]], [1, 1])
    coil(1010 + 3, False)                  # off by the same combo coil
    check("window 3 off, window 5 untouched", lit(want=0x10), 0x10)
    coil(COIL_ALL_OFF, True)
    check("All Off clears the display combos too",
          coils(1010 + 1, 8), [0] * 8)

    print("\n[17] latch+display combos 1031-1038 do not close siblings")
    coil(COIL_ENABLE + 1, True)            # a sibling that must survive
    lit(want=0x01)
    coil(1030 + 6, True)                   # window 6 + display + latch
    check("window 6 joined window 1", lit(want=0x21), 0x21)
    deadline = time.monotonic() + 4.0
    while coils(1030 + 6, 1)[0] and time.monotonic() < deadline:
        time.sleep(0.2)
    check("combo coil self-cleared", coils(1030 + 6, 1), [0])
    check("window 6 enable coil synced", coils(COIL_ENABLE + 6, 1), [1])
    check("window 1 still lit throughout", lit(want=0x21), 0x21)
    coil(COIL_ALL_OFF, True)
    lit(want=0)

    print("\n[15] churn: 60 random toggles, the bitmask must track exactly")
    rng = random.Random(20260907)
    want = 0
    coil(COIL_ALL_OFF, True)
    lit(want=0)
    drift = 0
    for i in range(60):
        n = rng.randint(1, 8)
        on = rng.random() < 0.55
        coil(COIL_ENABLE + n, on)
        want = (want | (1 << (n - 1))) if on else (want & ~(1 << (n - 1)))
        time.sleep(0.12)
    time.sleep(2.0)
    got = reg(REG_LIT)
    check("final bitmask matches the command sequence", got, want)
    check("coils agree with the bitmask",
          coils(COIL_ENABLE + 1, 8),
          [(want >> b) & 1 for b in range(8)])
    coil(COIL_ALL_OFF, True)
    check("All Off ends clean", lit(want=0), 0)


if __name__ == "__main__":
    sys.exit(main())
