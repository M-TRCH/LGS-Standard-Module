"""Build the one-file factory image: bootloader + app, flashable at 0x08000000.

Initial installation over ST-Link needs both stages, and doing it as two
PlatformIO uploads is two chances to forget one. This produces the single file
STM32CubeProgrammer wants, the same way the current asset was made by hand —
except the recipe now lives here instead of in a commit message.

    python tools/make_factory_image.py                 # write into assets/
    python tools/make_factory_image.py --out my.bin    # somewhere else

Verifies that the app carries exactly one valid commissioning block, so an
image that cannot be commissioned never reaches assets/.
"""
from __future__ import annotations

import argparse
import hashlib
import re
import struct
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOOT_BIN = ROOT / ".pio" / "build" / "LGS_BOOT" / "firmware.bin"
APP_BIN = ROOT / ".pio" / "build" / "LGS_STM32G070CBT6" / "firmware.bin"
BOOT_SLOT = 0x1000                      # app links at this offset (flash_layout.h)
PAD = 0xFF                              # erased flash

MAGIC = b"LGS-COMMISSION"
BLOCK_SIZE = 32
RAM_START, RAM_END = 0x20000000, 0x20009000
APP_ADDR = 0x08001000


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def valid_blocks(image: bytes) -> list[int]:
    """Offsets of commissioning blocks that pass version, size and CRC.

    The magic also appears as the string literal the firmware compares
    against, so presence alone does not identify the block.
    """
    out, start = [], 0
    while True:
        i = image.find(MAGIC, start)
        if i < 0:
            return out
        start = i + 1
        block = image[i:i + BLOCK_SIZE]
        if len(block) < BLOCK_SIZE:
            continue
        version, size = struct.unpack_from("<2H", block, 16)
        crc = struct.unpack_from("<H", block, 30)[0]
        if version == 1 and size == BLOCK_SIZE and crc16_ccitt(block[:30]) == crc:
            out.append(i)


def fw_version() -> str:
    text = (ROOT / "include" / "version.h").read_text(encoding="utf-8")
    m = re.search(r"#define\s+FW_VERSION\s+(\d+)", text)
    return m.group(1) if m else "unknown"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=None)
    args = ap.parse_args()

    for path in (BOOT_BIN, APP_BIN):
        if not path.exists():
            print(f"missing {path.relative_to(ROOT)}")
            print("Build both environments first:")
            print("  pio run -e LGS_BOOT")
            print("  pio run -e LGS_STM32G070CBT6")
            return 2

    boot = BOOT_BIN.read_bytes()
    app = APP_BIN.read_bytes()

    if len(boot) > BOOT_SLOT:
        print(f"bootloader is {len(boot)} B, over its {BOOT_SLOT} B slot")
        return 1

    # The app must look like a vector table for this offset, or the bootloader
    # will refuse to jump to it (src/boot/boot.c) and the board idles forever.
    sp, pc = struct.unpack_from("<2I", app, 0)
    if not (RAM_START <= sp <= RAM_END):
        print(f"app stack pointer 0x{sp:08X} is not in RAM — wrong build?")
        return 1
    if not (APP_ADDR <= pc < 0x08010800):
        print(f"app entry 0x{pc:08X} is not in the app slot — wrong flash_offset?")
        return 1

    blocks = valid_blocks(app)
    if len(blocks) != 1:
        print(f"app carries {len(blocks)} valid commissioning blocks, expected 1")
        print("Run pio run -e LGS_STM32G070CBT6 and read the post-build check.")
        return 1

    image = boot + bytes([PAD]) * (BOOT_SLOT - len(boot)) + app

    out = args.out
    if out is None:
        out = (ROOT / "assets" /
               f"firmware_stm32g070_v{fw_version()}_factory_{date.today():%Y-%m-%d}.bin")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(image)

    print(f"boot {len(boot):>6} B  (padded to {BOOT_SLOT})")
    print(f"app  {len(app):>6} B  (commissioning block at 0x{blocks[0]:05X})")
    print(f"->   {len(image):>6} B  {out.relative_to(ROOT) if out.is_relative_to(ROOT) else out}")
    print(f"sha256 {hashlib.sha256(image).hexdigest()}")
    print("Flash at 0x08000000.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
