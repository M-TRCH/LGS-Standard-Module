#!/usr/bin/env python3
"""
OTA Firmware Sender for STM32F103 via RS485 / Serial
=====================================================
Usage:
    python ota_sender.py -p COM3 -f firmware.bin
    python ota_sender.py -p COM3                     # opens file dialog
    python ota_sender.py -p /dev/ttyUSB0 -f fw.bin -b 115200

Requirements:
    pip install pyserial
"""

import argparse
import os
import struct
import sys
import time

try:
    import serial
except ImportError:
    print("Error: pyserial not installed.  Run:  pip install pyserial")
    sys.exit(1)

# ── Protocol constants ──────────────────────────────────────
SYNC            = bytes([0xAA, 0x55])
CMD_START       = 0x01
CMD_DATA        = 0x02
CMD_END         = 0x03

PACKET_SIZE     = 128       # Max data bytes per DATA packet
MAX_FW_SIZE     = 0x7C00    # 31 KB (matches OTA_MAX_FW_SIZE in firmware)

# Timing delays for broadcast mode (no ACK)
DELAY_AFTER_START   = 3.0   # seconds — wait for flash erase (~32 pages)
DELAY_BETWEEN_DATA  = 0.05  # seconds — wait for flash write
DELAY_AFTER_END     = 2.0   # seconds — wait for CRC verify + copy

# ── CRC-16/MODBUS ───────────────────────────────────────────
def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else crc >> 1
    return crc & 0xFFFF

# ── CRC-32 (ISO-HDLC) ──────────────────────────────────────
def crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xEDB88320 if (crc & 1) else crc >> 1
    return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFF

# ── Frame helpers ───────────────────────────────────────────
def build_frame(cmd: int, payload: bytes) -> bytes:
    length = len(payload)
    header = bytes([0xAA, 0x55, cmd, length & 0xFF, (length >> 8) & 0xFF])
    crc_data = bytes([cmd, length & 0xFF, (length >> 8) & 0xFF]) + payload
    c = crc16(crc_data)
    return header + payload + struct.pack('<H', c)


# ── File selection ──────────────────────────────────────────
def pick_file() -> str:
    """Open a tkinter file dialog, return path or empty."""
    try:
        import tkinter as tk
        from tkinter import filedialog
        root = tk.Tk()
        root.withdraw()
        path = filedialog.askopenfilename(
            title="Select firmware binary",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")]
        )
        root.destroy()
        return path if path else ""
    except Exception:
        return ""

# ── Main ────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="OTA Firmware Sender for STM32F103 via RS485/Serial")
    parser.add_argument('-p', '--port', required=True, help='COM port (e.g. COM3)')
    parser.add_argument('-f', '--file', default=None, help='Firmware .bin file')
    parser.add_argument('-b', '--baud', type=int, default=9600, help='Baud rate (default 9600)')
    args = parser.parse_args()

    # ── Select firmware file ──
    fw_path = args.file
    if not fw_path:
        print("No --file specified, opening file dialog...")
        fw_path = pick_file()
    if not fw_path or not os.path.isfile(fw_path):
        print(f"Error: firmware file not found: {fw_path}")
        sys.exit(1)

    with open(fw_path, 'rb') as f:
        firmware = f.read()
    fw_size = len(firmware)

    if fw_size == 0 or fw_size > MAX_FW_SIZE:
        print(f"Error: size {fw_size} bytes — max {MAX_FW_SIZE} bytes ({MAX_FW_SIZE//1024} KB)")
        sys.exit(1)

    fw_crc = crc32(firmware)

    print(f"  File : {os.path.basename(fw_path)}")
    print(f"  Size : {fw_size:,} bytes ({fw_size/1024:.1f} KB)")
    print(f"  CRC32: 0x{fw_crc:08X}")
    print(f"  Port : {args.port} @ {args.baud} baud")
    print()

    # ── Open serial ──
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except Exception as e:
        print(f"Error opening {args.port}: {e}")
        sys.exit(1)

    time.sleep(0.3)
    ser.reset_input_buffer()

    t_start = time.time()

    # ── Step 0: Modbus RTU broadcast — enter OTA mode ──
    print("[0/3] MODBUS — broadcast write coil 505 = ON (enter OTA mode)...")
    #  Slave=0x00(broadcast), Func=0x05(WriteSingleCoil), Addr=0x01F9(505), Value=0xFF00(ON)
    modbus_pdu = bytes([0x00, 0x05, 0x01, 0xF9, 0xFF, 0x00])
    modbus_crc = crc16(modbus_pdu)
    modbus_frame = modbus_pdu + struct.pack('<H', modbus_crc)
    ser.write(modbus_frame)
    print(f"  Waiting 3.0s for device to enter OTA mode...")
    time.sleep(3.0)
    print("  Done")

    # ── Step 1: START ──
    print("[1/3] START — sending firmware size...")
    frame = build_frame(CMD_START, struct.pack('<I', fw_size))
    ser.write(frame)
    print(f"  Waiting {DELAY_AFTER_START:.1f}s for flash erase...")
    time.sleep(DELAY_AFTER_START)
    print("  Done")

    # ── Step 2: DATA ──
    total_packets = (fw_size + PACKET_SIZE - 1) // PACKET_SIZE
    print(f"[2/3] DATA — broadcasting {total_packets} packets...")

    for seq in range(total_packets):
        offset = seq * PACKET_SIZE
        chunk = firmware[offset:offset + PACKET_SIZE]
        payload = struct.pack('<H', seq) + chunk
        frame = build_frame(CMD_DATA, payload)
        ser.write(frame)
        time.sleep(DELAY_BETWEEN_DATA)

        # Progress bar
        pct = (seq + 1) / total_packets * 100
        bar_w = 40
        filled = int(bar_w * (seq + 1) / total_packets)
        bar = '\u2588' * filled + '\u2591' * (bar_w - filled)
        print(f"\r  [{bar}] {pct:5.1f}%  ({seq+1}/{total_packets})", end='', flush=True)

    print()  # newline after progress bar

    # ── Step 3: END ──
    print("[3/3] END  — sending CRC32 for verification...")
    frame = build_frame(CMD_END, struct.pack('<I', fw_crc))
    ser.write(frame)
    print(f"  Waiting {DELAY_AFTER_END:.1f}s for CRC verify + copy...")
    time.sleep(DELAY_AFTER_END)

    elapsed = time.time() - t_start
    print()
    print(f"  BROADCAST COMPLETE!  {fw_size:,} bytes sent in {elapsed:.1f}s")
    print(f"  All devices will verify CRC32, copy firmware and reset.")

    ser.close()


if __name__ == '__main__':
    main()
