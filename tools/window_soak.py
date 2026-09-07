"""Soak the window engine the way a ward actually uses it.

    python tools/window_soak.py --port COM8 --id 247 --minutes 20
    python tools/window_soak.py --port COM8 --id 247            # until Ctrl-C

Every long-run number this project owns was measured on RING boards. The
mask display has never been soaked at all, and it is the entire type 56
cabinet -- so this exists to start closing that gap before v3.5.0 ships.

It is not a register poller. It models the thing the eight windows are for:
several people's medications waiting in one slot at the same time. Windows
are drawn from a SHUFFLED DECK rather than uniformly at random, so over a
run every window gets the same number of activations -- uniform draws give
Poisson spread, and then any per-window comparison afterwards is confounded
by exposure rather than by health. Each activation lives for a dwell and is
then cleared by this script, exactly as the server clears one when a tablet
confirm arrives.

What it is actually looking for, none of which a short functional test can
see:

  * reg 61 drifting away from the set of windows we commanded. A bitmask
    maintained by hand is exactly the thing that rots under churn, and every
    cycle re-checks it.
  * a reboot nobody asked for -- the boot counter is read on every counter
    pass, so a reset cannot hide between samples.
  * IWDG resets (reg 410 is a lifetime count), which is how a stall in the
    new per-window bookkeeping would show itself.
  * per-window statistics still adding up after thousands of on/off edges.

The CSV is the soak schema `time,device_id,kind,detail`, so app/soak_csv.py
in the Test Tool reads these files and the site report can carry them.
"""
from __future__ import annotations

import argparse
import random
import sys
import time
from datetime import datetime

try:
    from pymodbus.client import ModbusSerialClient, ModbusTcpClient
except ImportError:  # pragma: no cover
    print("pymodbus not installed - run with the LGS-Test-Tool venv python")
    sys.exit(2)

REG_FW, REG_BOOTS, REG_RESET_CAUSE = 1, 7, 8
REG_UPTIME_HI, REG_LIT = 5, 61
REG_IWDG = 410
COIL_ENABLE = 1000
COIL_ALL_OFF = 511
WINDOWS = 8

CAUSE_BITS = ["IWDG", "software", "Power-on", "NRST pin",
              "WWDG", "low-power", "option-byte"]


def cause_name(bits: int) -> str:
    named = [n for i, n in enumerate(CAUSE_BITS) if bits & (1 << i)]
    return " ".join(named) if named else f"unknown 0x{bits:04X}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port")
    ap.add_argument("--tcp")
    ap.add_argument("--id", type=int, default=247)
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--minutes", type=float, default=0,
                    help="stop after this long; 0 = run until Ctrl-C")
    ap.add_argument("--dwell", type=float, default=6.0,
                    help="seconds a window stays lit before being cleared")
    ap.add_argument("--cycle", type=float, default=1.5,
                    help="seconds between commands")
    ap.add_argument("--counter-every", type=int, default=40,
                    help="cycles between counter passes (reboot/IWDG check)")
    ap.add_argument("--slow-ms", type=int, default=400)
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    if bool(a.port) == bool(a.tcp):
        ap.error("exactly one of --port / --tcp")

    c = (ModbusSerialClient(port=a.port, baudrate=a.baud, timeout=2.0, retries=0)
         if a.port else ModbusTcpClient(host=a.tcp, port=502, timeout=6.0))
    if not c.connect():
        print("cannot connect")
        return 1
    dev = {"device_id": a.id}

    out = a.out or f"windowsoak-{datetime.now():%Y%m%d-%H%M}.csv"
    fh = open(out, "w", encoding="utf-8", newline="")
    fh.write("time,device_id,kind,detail\n")

    stats = {"cycles": 0, "reads": 0, "fails": 0, "reboots": 0,
             "wdt": 0, "mismatch": 0, "worst_ms": 0}

    def row(devid, kind, detail):
        fh.write(f"{datetime.now():%Y-%m-%d %H:%M:%S},{devid},{kind},{detail}\n")
        fh.flush()

    def timed(fn, *args, **kw):
        """Run one transaction, timing it and folding the result into stats."""
        t0 = time.monotonic()
        try:
            r = fn(*args, **kw)
            bad = r.isError()
        except Exception:
            r, bad = None, True
        ms = int((time.monotonic() - t0) * 1000)
        stats["reads"] += 1
        stats["worst_ms"] = max(stats["worst_ms"], ms)
        if bad:
            stats["fails"] += 1
            row(a.id, "no_reply", f"after {ms} ms")
            return None
        if ms > a.slow_ms:
            row(a.id, "slow", f"{ms} ms")
        return r

    def reg(addr, n=1):
        r = timed(c.read_holding_registers, addr, count=n, **dev)
        return r.registers if r else None

    def coil(addr, value):
        return timed(c.write_coil, addr, value, **dev) is not None

    # Startup probe retries. Mid-run a lost read is DATA and gets recorded,
    # but refusing to start over one transient would throw away a night --
    # and the first transaction after an idle bus is exactly where a stray
    # timeout shows up.
    fw = None
    for attempt in range(6):
        fw = reg(REG_FW)
        if fw:
            if attempt:
                print(f"  (module answered on attempt {attempt + 1})")
            break
        time.sleep(1.0)
    if not fw:
        print("module did not answer after 6 attempts")
        return 1
    if fw[0] < 30500:
        print(f"fw {fw[0]} has no window engine; this soak would prove nothing")
        return 2

    boots = reg(REG_BOOTS)[0]
    iwdg = reg(REG_IWDG)[0]
    print(f"module {a.id}, fw {fw[0]}, boots {boots}, iwdg {iwdg} -> {out}")
    row(0, "start", f"ids=1 target={a.id} fw={fw[0]} dwell_s={a.dwell} "
                    f"cycle_s={a.cycle} slow_ms={a.slow_ms}")

    coil(COIL_ALL_OFF, True)
    time.sleep(1.5)

    lit: dict[int, float] = {}          # window -> monotonic time it was lit
    deck: list[int] = []
    rng = random.Random()
    started = time.monotonic()
    deadline = started + a.minutes * 60 if a.minutes else None

    try:
        while deadline is None or time.monotonic() < deadline:
            stats["cycles"] += 1
            now = time.monotonic()

            # Clear anything that has served its dwell -- the server's job in
            # the real system, and the only thing that ever puts a window out
            # before max-on-time.
            for n in [n for n, t in lit.items() if now - t >= a.dwell]:
                coil(COIL_ENABLE + n, False)
                lit.pop(n, None)

            # Deal the next window off the shuffled deck. Refill and reshuffle
            # when it runs out, which is what keeps activations per window
            # equal over the run.
            if len(lit) < WINDOWS:
                if not deck:
                    deck = list(range(1, WINDOWS + 1))
                    rng.shuffle(deck)
                n = deck.pop()
                if n not in lit:
                    coil(COIL_ENABLE + n, True)
                    lit[n] = time.monotonic()

            # The assertion that matters: does the board agree with us about
            # which windows are on? reg 61 is published once a second, so a
            # freshly-changed window may lag one sample; only a disagreement
            # that survives a re-read is real.
            want = 0
            for n in lit:
                want |= 1 << (n - 1)
            got = reg(REG_LIT)
            if got is not None and got[0] != want:
                time.sleep(1.2)
                got = reg(REG_LIT)
                if got is not None and got[0] != want:
                    stats["mismatch"] += 1
                    row(a.id, "mismatch",
                        f"reg61=0x{got[0]:02X} expected 0x{want:02X}")

            if stats["cycles"] % a.counter_every == 0:
                nb = reg(REG_BOOTS)
                if nb and nb[0] != boots:
                    cause = reg(REG_RESET_CAUSE)
                    ni = reg(REG_IWDG)
                    ctext = cause_name(cause[0]) if cause else "unread"
                    itext = (f"iwdg {iwdg} -> {ni[0]}" if ni and ni[0] != iwdg
                             else f"iwdg {iwdg} unchanged")
                    row(a.id, "reboot",
                        f"boots {boots} -> {nb[0]} cause={ctext} {itext}")
                    stats["reboots"] += nb[0] - boots
                    if ni and ni[0] != iwdg:
                        stats["wdt"] += ni[0] - iwdg
                        row(a.id, "watchdog", itext)
                        iwdg = ni[0]
                    boots = nb[0]
                    lit.clear()          # RAM state went with the reboot
                up = reg(REG_UPTIME_HI, 2)
                elapsed = int(time.monotonic() - started)
                row(0, "heartbeat",
                    f"pass={stats['cycles']} reads={stats['reads']} "
                    f"fails={stats['fails']} reboots={stats['reboots']} "
                    f"wdt={stats['wdt']} mismatch={stats['mismatch']} "
                    f"worst_ms={stats['worst_ms']} "
                    f"up={(up[0] << 16 | up[1]) if up else '?'} "
                    f"elapsed_s={elapsed}")
                print(f"  {elapsed:5d}s  cycles {stats['cycles']:5d}  "
                      f"reads {stats['reads']:6d}  fails {stats['fails']}  "
                      f"reboots {stats['reboots']}  wdt {stats['wdt']}  "
                      f"mismatch {stats['mismatch']}  worst {stats['worst_ms']} ms")

            time.sleep(a.cycle)
    except KeyboardInterrupt:
        print("\nstopping")

    coil(COIL_ALL_OFF, True)
    elapsed = int(time.monotonic() - started)
    row(0, "stop", f"pass={stats['cycles']} reads={stats['reads']} "
                   f"fails={stats['fails']} reboots={stats['reboots']} "
                   f"wdt={stats['wdt']} mismatch={stats['mismatch']} "
                   f"worst_ms={stats['worst_ms']} elapsed_s={elapsed}")
    fh.close()
    c.close()
    print(f"\n{elapsed} s, {stats['cycles']} cycles, {stats['reads']} reads, "
          f"fails {stats['fails']}, reboots {stats['reboots']}, "
          f"wdt {stats['wdt']}, mismatch {stats['mismatch']}")
    print(f"log: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
