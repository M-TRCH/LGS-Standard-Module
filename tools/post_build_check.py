"""Fail the build unless the commissioning block survived into firmware.bin.

The block (include/commission.h) is the only thing a host tool can patch to
give a blank board its Modbus ID, and it is a plain `const` object read once
at boot — exactly the shape a link-time optimiser is entitled to fold into the
code and drop. `used` + `volatile` + external linkage discourage that today,
but nothing in the language guarantees it across toolchain versions.

So the guarantee lives here instead: if the block is missing, duplicated, or
inconsistent, the build fails. A silent regression becomes a red build rather
than a batch of boards that cannot be commissioned.

Wired in from platformio.ini as an extra_script; PlatformIO calls it with the
build environment in scope.
"""
Import("env")  # noqa: F821  — injected by PlatformIO/SCons

import struct
import sys
from pathlib import Path

MAGIC_TEXT = b"LGS-COMMISSION"         # the block stores it NUL-padded in char[16]
BLOCK_SIZE = 32
CRC_LEN = BLOCK_SIZE - 2               # every byte except the trailing crc
VERSION = 1


def crc16_ccitt(data: bytes) -> int:
    """Poly 0x1021, init 0xFFFF — the same CRC the settings blob uses."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def fail(message: str) -> None:
    print("")
    print("=" * 72)
    print("COMMISSIONING BLOCK CHECK FAILED")
    print(message)
    print("=" * 72)
    sys.exit(1)


def check(source, target, env):  # noqa: ARG001 — SCons signature
    path = Path(env.subst("$BUILD_DIR")) / "firmware.bin"
    if not path.exists():
        return                          # nothing to check (e.g. a clean)

    data = path.read_bytes()

    # The magic appears more than once on purpose: once in the block, and once
    # as the string literal commissionRead() compares against. So "exactly one
    # magic" is the wrong rule — "exactly one candidate that validates" is the
    # one that matters, and it stays true however the compiler lays out
    # literals.
    candidates = []
    near_misses = []
    start = 0
    while True:
        i = data.find(MAGIC_TEXT, start)
        if i < 0:
            break
        start = i + 1
        block = data[i:i + BLOCK_SIZE]
        if len(block) < BLOCK_SIZE:
            continue
        version, size = struct.unpack_from("<2H", block, 16)
        crc = struct.unpack_from("<H", block, 30)[0]
        if version != VERSION or size != BLOCK_SIZE:
            continue
        if crc16_ccitt(block[:CRC_LEN]) != crc:
            near_misses.append((i, crc, crc16_ccitt(block[:CRC_LEN])))
            continue
        candidates.append((i, block))

    if not candidates and near_misses:
        off, got, want = near_misses[0]
        fail(f"A block is present at 0x{off:06X} but its CRC is 0x{got:04X} "
             f"while the contents hash to 0x{want:04X}.\n"
             f"Set .crc = 0x{want:04X} in src/svc/commission.cpp and rebuild.\n"
             "(Expected the first time, or whenever a default the block carries\n"
             "— such as DEFAULT_IDENTIFIER — changes.)")
    if not candidates:
        fail("No valid commissioning block is in firmware.bin.\n"
             "The optimiser most likely dropped gCommissionBlock. Check that\n"
             "src/svc/commission.cpp still declares it `const volatile` with\n"
             "external linkage and `used`, and that the file is still compiled.\n"
             "If a toolchain ever defeats that for good, move the definition\n"
             "into hand-written assembly — LTO cannot see into it.")
    if len(candidates) > 1:
        fail(f"{len(candidates)} valid blocks at "
             f"{', '.join(hex(o) for o, _ in candidates)}.\n"
             "A host tool cannot know which one to patch. The block must be\n"
             "defined in exactly one translation unit — check whether the\n"
             "definition moved into a header.")

    offset, block = candidates[0]
    version, size, token_lo, token_hi, apply_mask, flags, identifier, crc = \
        struct.unpack_from("<8H", block, 16)
    token = (token_hi << 16) | token_lo

    if token != 0:
        fail(f"Block token is {token}, expected 0 as built. A non-zero token\n"
             "means this image would commission a board on first boot; only a\n"
             "host tool patching a copy may set it.")

    # What this check cannot prove: that the code actually *reads* the block
    # rather than using values the optimiser folded into the instructions. A
    # literal-pool canary was tried and rejected — the compiler legitimately
    # loads one nearby literal and reaches the block with an immediate add
    # (186 bytes here, well inside Cortex-M0+'s ADD imm8), so a correct build
    # can carry no absolute reference at all and the canary fired on every
    # good build. A gate that cries wolf gets switched off, and then guards
    # nothing.
    #
    # Folding is held off by `const volatile` plus the byte-wise read in
    # commissionRead(); the proof it worked is the hardware test — patch an
    # ID, flash, confirm the board adopts it. Re-run that after any toolchain
    # bump, alongside the boot/ISR check platformio.ini already calls for.
    flash_addr = 0x08001000 + offset
    print(f"commissioning block: ok at 0x{offset:06X} (flash 0x{flash_addr:08X}), "
          f"id={identifier}, crc=0x{crc:04X}")


env.AddPostAction("$BUILD_DIR/firmware.bin", check)  # noqa: F821
