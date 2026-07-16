#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LGS R5.0 - OTA firmware sender over RS485 (Modbus broadcast)
============================================================
Streams a firmware .bin to every LGS board on the bus at once using Modbus
broadcast FC16 writes (slave id 0), then verifies and applies per device.

Easiest way - just run it with no arguments for the interactive menu
(pick port/baud/devices once, then send updates via a file-browse dialog):

  & "$env:USERPROFILE\\miniconda3\\python.exe" tools/ota_sender.py

Scriptable CLI (unchanged, for automation):

  ... tools/ota_sender.py -p COM30 --ids 21 -f .pio/build/LGS_STM32G070CBT6/firmware.bin -y
  ... tools/ota_sender.py -p COM30 --ids 21 --status
  ... tools/ota_sender.py -p COM30 --abort

Flow (device side: src/app/ota_control.cpp, layout: include/flash_layout.h):
  probe -> broadcast metadata + coil 505 (staging erase) -> stream 128B chunks
  -> per-device bitmap repair rounds -> coil 506 verify -> coil 507 apply
  (bootloader copies staging -> app slot) -> confirm new FW version (reg 1).

The image must be built for the app slot (board_build.flash_offset=0x1000)
and be <= 61,440 bytes. Requirements: pymodbus>=3.7, pyserial.
"""

import argparse
import logging
import os
import sys
import time
import zlib

try:
    from pymodbus.client import ModbusSerialClient
except ImportError:
    print("Error: pymodbus not installed.  Run:  pip install pymodbus pyserial")
    sys.exit(1)

try:
    from serial.tools import list_ports as serial_list_ports
except ImportError:
    serial_list_ports = None

logging.getLogger("pymodbus").setLevel(logging.CRITICAL)

# --- Wire contract (mirrors src/svc/modbus_map.h + include/flash_layout.h) --
REG_STATE        = 282   # lo=state (0 idle/1 rx/2 verified/3 failed), hi=error
REG_CHUNKS_RX    = 283
REG_META_FIRST   = 284   # size_hi, size_lo, crc_hi, crc_lo, total_chunks
REG_CHUNK_FIRST  = 290   # index, len, crc16, data x64, commit  (68 regs)
REG_BITMAP_FIRST = 360
BITMAP_REGS      = 30
COIL_ENTER, COIL_FINALIZE, COIL_APPLY, COIL_ABORT = 505, 506, 507, 508
CHUNK_SIZE       = 128
MAX_IMAGE_SIZE   = 61440
BAUD_CHOICES     = (9600, 19200, 38400, 57600)

STATE_NAMES = {0: "idle", 1: "receiving", 2: "verified", 3: "failed"}
ERROR_NAMES = {0: "-", 1: "bad size", 2: "bad chunk count", 3: "image CRC32 mismatch",
               4: "session timeout", 5: "flash write error", 6: "apply while not verified",
               7: "latch busy", 8: "chunks incomplete"}

DEFAULT_BIN_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                               ".pio", "build", "LGS_STM32G070CBT6")


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc


class OtaSession:
    def __init__(self, client, ids, gap_s):
        self.c = client
        self.ids = ids
        self.gap = gap_s
        self.tx_counter = 0

    # --- low-level helpers -------------------------------------------------
    def bcast_regs(self, addr, values):
        self.c.write_registers(addr, values, device_id=0, no_response_expected=True)
        time.sleep(self.gap)

    def bcast_coil(self, addr):
        self.c.write_coil(addr, True, device_id=0, no_response_expected=True)
        time.sleep(self.gap)

    def read_regs(self, uid, addr, count):
        try:
            r = self.c.read_holding_registers(addr, count=count, device_id=uid)
            return r.registers if (r and not r.isError()) else None
        except Exception:
            return None

    def state_of(self, uid):
        r = self.read_regs(uid, REG_STATE, 2)
        if r is None:
            return None
        return {"state": r[0] & 0xFF, "error": r[0] >> 8, "chunks": r[1]}

    # --- chunk streaming ----------------------------------------------------
    def send_chunk(self, image, idx):
        offset = idx * CHUNK_SIZE
        payload = image[offset:offset + CHUNK_SIZE]
        length = len(payload)
        padded = payload + b"\xff" * (-length % 2)
        data_regs = [(padded[i] << 8) | padded[i + 1] for i in range(0, len(padded), 2)]
        data_regs += [0xFFFF] * (64 - len(data_regs))
        self.tx_counter = (self.tx_counter + 1) & 0xFFFF
        frame = [idx, length, crc16_ccitt(payload)] + data_regs + [self.tx_counter]
        self.bcast_regs(REG_CHUNK_FIRST, frame)

    def missing_chunks(self, uid, total_chunks):
        regs = self.read_regs(uid, REG_BITMAP_FIRST, BITMAP_REGS)
        if regs is None:
            return None
        return [idx for idx in range(total_chunks)
                if not (regs[idx // 16] >> (idx % 16)) & 1]


def human(n):
    return f"{n:,}"


# ---------------------------------------------------------------------------
# UI helpers
# ---------------------------------------------------------------------------

def pick_file():
    """Open a file-browse dialog for the firmware .bin; '' if cancelled."""
    try:
        import tkinter as tk
        from tkinter import filedialog
        root = tk.Tk()
        root.withdraw()
        root.attributes("-topmost", True)
        path = filedialog.askopenfilename(
            title="Select firmware .bin (built for the app slot, offset 0x1000)",
            initialdir=DEFAULT_BIN_DIR if os.path.isdir(DEFAULT_BIN_DIR) else None,
            filetypes=[("Firmware binary", "*.bin"), ("All files", "*.*")])
        root.destroy()
        return path or ""
    except Exception:
        # No tkinter / headless: fall back to a plain prompt
        try:
            return input("  path to firmware .bin> ").strip().strip('"')
        except EOFError:
            return ""


def available_ports():
    if serial_list_ports is None:
        return []
    return [(p.device, p.description) for p in serial_list_ports.comports()]


def open_client(port, baud):
    client = ModbusSerialClient(port=port, baudrate=baud, bytesize=8,
                                parity="N", stopbits=1, timeout=1.0, retries=1)
    return client if client.connect() else None


# ---------------------------------------------------------------------------
# Actions (shared by CLI and the interactive menu)
# ---------------------------------------------------------------------------

def action_status(client, ids):
    s = OtaSession(client, ids, 0.02)
    for uid in ids:
        st = s.state_of(uid)
        if st is None:
            print(f"  id {uid}: no reply")
        else:
            print(f"  id {uid}: {STATE_NAMES.get(st['state'], '?')} "
                  f"(error: {ERROR_NAMES.get(st['error'], st['error'])}, chunks {st['chunks']})")
    return 0


def action_abort(client, gap_s):
    OtaSession(client, [], gap_s).bcast_coil(COIL_ABORT)
    print("broadcast OTA abort sent")
    return 0


def action_send(client, ids, image_path, *, gap_s, repair_rounds, broadcast_apply,
                yes, drop_every=0):
    try:
        image = open(image_path, "rb").read()
    except OSError as e:
        print(f"[ERR] cannot read {image_path}: {e}")
        return 2
    if not (8 <= len(image) <= MAX_IMAGE_SIZE):
        print(f"[ERR] image is {human(len(image))} B; OTA cap is {human(MAX_IMAGE_SIZE)} B")
        return 2

    s = OtaSession(client, ids, gap_s)
    crc32 = zlib.crc32(image) & 0xFFFFFFFF
    total_chunks = (len(image) + CHUNK_SIZE - 1) // CHUNK_SIZE
    print(f"image: {image_path}")
    print(f"  size {human(len(image))} B, CRC32 {crc32:08X}, {total_chunks} chunks of {CHUNK_SIZE} B")

    # 1. PROBE
    print(f"\n[1/8] probing devices {ids} ...")
    old_fw = {}
    for uid in ids:
        r = s.read_regs(uid, 0, 5)
        if r is None:
            print(f"  id {uid}: NO REPLY - aborting (fix the bus or drop it from the device list)")
            return 2
        old_fw[uid] = r[1]
        print(f"  id {uid}: type {r[0]}, FW {r[1]}, HW {r[2]}")

    if not yes:
        try:
            ans = input(f"\n  Flash {human(len(image))} B to {len(ids)} device(s) over the bus? [y/N] ")
        except EOFError:
            ans = ""
        if ans.strip().lower() not in ("y", "yes"):
            print("  cancelled")
            return 1

    # 2+3. METADATA + ENTER
    print("[2/8] broadcasting metadata ...")
    s.bcast_regs(REG_META_FIRST, [len(image) >> 16, len(image) & 0xFFFF,
                                  crc32 >> 16, crc32 & 0xFFFF, total_chunks])
    print("[3/8] entering OTA mode (staging erase ~1s) ...")
    s.bcast_coil(COIL_ENTER)
    time.sleep(2.0)
    for uid in ids:
        st = s.state_of(uid)
        if st is None or st["state"] != 1:
            print(f"  id {uid}: did not enter OTA "
                  f"({'no reply' if st is None else ERROR_NAMES.get(st['error'], st['error'])})")
            return 2
    print(f"  all {len(ids)} device(s) receiving")

    # 4. STREAM
    print(f"[4/8] streaming {total_chunks} chunks ...")
    t0 = time.time()
    for idx in range(total_chunks):
        if drop_every and idx % drop_every == drop_every - 1:
            continue  # TEST: simulate a lost broadcast frame
        s.send_chunk(image, idx)
        if idx % 32 == 31 or idx == total_chunks - 1:
            pct = (idx + 1) * 100 // total_chunks
            print(f"\r  {idx + 1}/{total_chunks}  ({pct}%)  "
                  f"{time.time() - t0:.0f}s", end="", flush=True)
    print()

    # 5. REPAIR
    print("[5/8] bitmap check + repair ...")
    for round_no in range(1, repair_rounds + 1):
        union_missing = set()
        for uid in ids:
            miss = s.missing_chunks(uid, total_chunks)
            if miss is None:
                print(f"  id {uid}: bitmap read failed")
                return 2
            if miss:
                print(f"  id {uid}: missing {len(miss)} chunk(s)")
            union_missing.update(miss)
        if not union_missing:
            print("  all devices report a complete image")
            break
        print(f"  repair round {round_no}: re-sending {len(union_missing)} chunk(s)")
        for idx in sorted(union_missing):
            s.send_chunk(image, idx)
    else:
        print("  [ERR] chunks still missing after all repair rounds")
        return 2

    # 6. FINALIZE
    print("[6/8] finalize (device-side CRC32) ...")
    s.bcast_coil(COIL_FINALIZE)
    time.sleep(1.0)
    verified = []
    for uid in ids:
        st = s.state_of(uid)
        name = STATE_NAMES.get(st["state"], "?") if st else "no reply"
        print(f"  id {uid}: {name}"
              + (f" (error: {ERROR_NAMES.get(st['error'], st['error'])})" if st and st["error"] else ""))
        if st and st["state"] == 2:
            verified.append(uid)
    if not verified:
        print("  [ERR] no device verified the image")
        return 2

    # 7. APPLY
    print(f"[7/8] applying to {verified} (reboot + bootloader copy ~4s) ...")
    if broadcast_apply:
        s.bcast_coil(COIL_APPLY)
    else:
        for uid in verified:
            try:
                s.c.write_coil(COIL_APPLY, True, device_id=uid)
            except Exception:
                pass  # the device may reset before answering
            time.sleep(0.1)
    time.sleep(5.0)

    # 8. CONFIRM
    print("[8/8] confirming new firmware version ...")
    ok = 0
    for uid in verified:
        r = None
        for _ in range(5):
            r = s.read_regs(uid, 1, 1)
            if r is not None:
                break
            time.sleep(1.0)
        if r is None:
            print(f"  id {uid}: no reply after reboot")
        else:
            changed = "UPDATED" if r[0] != old_fw[uid] else "same version"
            print(f"  id {uid}: FW {old_fw[uid]} -> {r[0]}  [{changed}]")
            ok += 1
    print(f"\nRESULT: {ok}/{len(verified)} device(s) running the new image")
    return 0 if ok == len(verified) else 1


# ---------------------------------------------------------------------------
# Interactive menu
# ---------------------------------------------------------------------------

def ask(prompt):
    try:
        return input(prompt).strip()
    except EOFError:
        return "0"


def change_settings(port, baud, ids):
    ports = available_ports()
    if ports:
        print("  available ports:")
        for i, (dev, desc) in enumerate(ports, 1):
            print(f"    {i}) {dev} - {desc}")
        sel = ask(f"  port (number or name) [{port}]> ")
        if sel.isdigit() and 1 <= int(sel) <= len(ports):
            port = ports[int(sel) - 1][0]
        elif sel:
            port = sel
    else:
        sel = ask(f"  port [{port}]> ")
        if sel:
            port = sel

    print("  baud rates: " + "  ".join(f"{i}) {b}" for i, b in enumerate(BAUD_CHOICES, 1)))
    sel = ask(f"  baud (number or value) [{baud}]> ")
    if sel.isdigit():
        v = int(sel)
        if 1 <= v <= len(BAUD_CHOICES):
            baud = BAUD_CHOICES[v - 1]
        elif v in BAUD_CHOICES:
            baud = v

    sel = ask(f"  device IDs, comma-separated [{','.join(map(str, ids))}]> ")
    if sel:
        try:
            ids = [int(x) for x in sel.split(",") if x.strip()]
        except ValueError:
            print("  invalid list, keeping the old one")

    return port, baud, ids


def interactive_menu(args):
    port = args.port
    baud = args.baud
    ids = [int(x) for x in args.ids.split(",") if x.strip()]

    while True:
        print()
        print("=" * 52)
        print("  LGS OTA Sender")
        print(f"  port {port} @ {baud} baud   devices {ids}")
        print("=" * 52)
        print("  1) Send firmware update  (browse for .bin)")
        print("  2) Read OTA status")
        print("  3) Abort OTA session")
        print("  4) Change settings (port / baud / device IDs)")
        print("  0) Exit")
        choice = ask("select> ")

        if choice == "0":
            return 0
        if choice == "4":
            port, baud, ids = change_settings(port, baud, ids)
            continue
        if choice not in ("1", "2", "3"):
            continue

        if choice == "1":
            path = pick_file()
            if not path:
                print("  no file selected")
                continue

        client = open_client(port, baud)
        if client is None:
            print(f"  [ERR] cannot open {port} (in use? unplugged?)")
            continue
        try:
            if choice == "1":
                action_send(client, ids, path,
                            gap_s=args.gap / 1000.0, repair_rounds=args.repair_rounds,
                            broadcast_apply=args.broadcast_apply, yes=False,
                            drop_every=args.drop_every)
            elif choice == "2":
                action_status(client, ids)
            else:
                action_abort(client, args.gap / 1000.0)
        except KeyboardInterrupt:
            print("\n  interrupted - the device session times out by itself (~30s)")
        finally:
            client.close()


# ---------------------------------------------------------------------------
# Entry
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="LGS R5.0 OTA firmware sender (Modbus RTU broadcast over RS485). "
                    "Run with no action arguments for the interactive menu.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    ap.add_argument("-p", "--port", default="COM30", help="USB-RS485 serial port")
    ap.add_argument("-b", "--baud", type=int, default=9600,
                    choices=BAUD_CHOICES, help="bus baud rate")
    ap.add_argument("-f", "--file", help="firmware .bin; omit to browse (CLI send) or use the menu")
    ap.add_argument("--ids", default="21",
                    help="comma-separated device IDs to verify/apply (e.g. 21,22,23)")
    ap.add_argument("--gap", type=float, default=25.0, help="inter-frame gap in ms")
    ap.add_argument("--repair-rounds", type=int, default=5, help="max bitmap repair rounds")
    ap.add_argument("--broadcast-apply", action="store_true",
                    help="apply with one broadcast instead of per-device unicast")
    ap.add_argument("--status", action="store_true", help="read OTA state per device and exit")
    ap.add_argument("--abort", action="store_true", help="broadcast OTA abort (coil 508) and exit")
    ap.add_argument("--send", action="store_true",
                    help="CLI send even without -f (opens the browse dialog)")
    ap.add_argument("-y", "--yes", action="store_true", help="skip the confirmation prompt")
    ap.add_argument("--drop-every", type=int, default=0, metavar="N",
                    help="TEST: skip every Nth chunk in the main stream so the "
                         "bitmap repair rounds have real work to do")
    args = ap.parse_args()

    # No action requested -> interactive menu.
    if not (args.status or args.abort or args.file or args.send):
        return interactive_menu(args)

    ids = [int(x) for x in args.ids.split(",") if x.strip()]
    client = open_client(args.port, args.baud)
    if client is None:
        print(f"[ERR] cannot open {args.port}")
        return 2
    try:
        if args.abort:
            return action_abort(client, args.gap / 1000.0)
        if args.status:
            return action_status(client, ids)

        path = args.file or pick_file()
        if not path:
            print("[ERR] no firmware file selected")
            return 2
        return action_send(client, ids, path,
                           gap_s=args.gap / 1000.0, repair_rounds=args.repair_rounds,
                           broadcast_apply=args.broadcast_apply, yes=args.yes,
                           drop_every=args.drop_every)
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
