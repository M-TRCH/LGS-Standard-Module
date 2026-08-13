"""Convert the fixed 53x64 digit bitmaps (oled_font_digits.h) into a compact
Adafruit GFXfont (oled_font_bignum.h) containing only glyphs '0'-'9'.

Each glyph is trimmed to its ink bounding box and bit-packed contiguously
(no per-row byte padding), which removes the blank columns of the fixed-cell
format. A built-in round-trip check asserts every generated glyph decodes
back to the grid it was packed from before the header is written.

The digits are also scaled down to CELL_HEIGHT. At the source's full 64 px
they filled the panel top to bottom and the bezel over the glass cut the
first and last rows — on the cabinet the digits read as clipped even though
every pixel really was being drawn. The scale is PROPORTIONAL: width follows
height, because digits squashed on one axis only look wrong long before you
can say why.

The resample is a 2-D box filter with a 50% threshold (an output pixel turns
on when at least half the source area it covers was ink), which keeps
strokes solid and gaps open — dropping rows and columns outright would thin
some strokes away and fatten others.

Usage:
    <python> tools/gen_oled_bignum.py               # CELL_HEIGHT
    <python> tools/gen_oled_bignum.py --height 48
"""

import argparse
import os
import re

HERE = os.path.dirname(__file__)
SRC = os.path.normpath(os.path.join(HERE, "..", "src", "drivers", "oled_font_digits.h"))
OUT = os.path.normpath(os.path.join(HERE, "..", "src", "drivers", "oled_font_bignum.h"))

DIGITS = "0123456789"
CELL_GAP = 6      # horizontal gap between tabular digit cells
CELL_HEIGHT = 54  # 64 px source - 10: clear of the bezel, still the big face


def parse_source():
    text = open(SRC, encoding="utf-8").read()
    width = int(re.search(r"OLED_DIGIT_WIDTH\s+(\d+)", text).group(1))
    height = int(re.search(r"OLED_DIGIT_HEIGHT\s+(\d+)", text).group(1))
    stride = int(re.search(r"OLED_DIGIT_STRIDE\s+(\d+)", text).group(1))
    # each glyph is a { ... } block; grab them in order
    blocks = re.findall(r"\{([^{}]*0x[^{}]*)\}", text)
    glyphs = []
    for b in blocks:
        vals = [int(v, 16) for v in re.findall(r"0x[0-9A-Fa-f]{2}", b)]
        if len(vals) == stride * height:
            glyphs.append(vals)
    assert len(glyphs) == 10, f"expected 10 digit blocks, got {len(glyphs)}"
    return width, height, stride, glyphs


def to_grid(data, width, height, stride):
    grid = [[0] * width for _ in range(height)]
    for y in range(height):
        for x in range(width):
            byte = data[y * stride + (x // 8)]
            grid[y][x] = (byte >> (7 - (x % 8))) & 1
    return grid


def resize_grid(grid, src_w, src_h, dst_w, dst_h):
    """Box-filter the grid to dst_w x dst_h, 50%-of-area threshold."""
    if (dst_w, dst_h) == (src_w, src_h):
        return grid
    sx = src_w / dst_w
    sy = src_h / dst_h
    cell_area = sx * sy
    out = []
    for y in range(dst_h):
        top = y * sy
        bottom = (y + 1) * sy
        row = [0] * dst_w
        for x in range(dst_w):
            left = x * sx
            right = (x + 1) * sx
            covered = 0.0
            for syi in range(int(top), min(src_h, int(bottom - 1e-9) + 1)):
                wy = min(syi + 1.0, bottom) - max(float(syi), top)
                if wy <= 0:
                    continue
                srow = grid[syi]
                for sxi in range(int(left), min(src_w, int(right - 1e-9) + 1)):
                    if not srow[sxi]:
                        continue
                    wx = min(sxi + 1.0, right) - max(float(sxi), left)
                    if wx > 0:
                        covered += wx * wy
            row[x] = 1 if covered >= cell_area / 2.0 else 0
        out.append(row)
    return out


def bbox(grid, width, height):
    minx, miny, maxx, maxy = width, height, -1, -1
    for y in range(height):
        for x in range(width):
            if grid[y][x]:
                minx = min(minx, x); maxx = max(maxx, x)
                miny = min(miny, y); maxy = max(maxy, y)
    return minx, miny, maxx, maxy


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--height", type=int, default=CELL_HEIGHT,
                    help="cell height in pixels after the vertical squash")
    args = ap.parse_args()

    src_width, src_height, stride, glyphs = parse_source()
    height = args.height
    # Width follows height so the digits keep their proportions.
    width = int(round(src_width * height / src_height))
    print(f"cell: {src_width}x{src_height} source -> {width}x{height} "
          f"({src_height - height} px shorter, scale {height / src_height:.3f})")

    trimmed = []   # (w, h, minx, miny, [packed bytes], grid_region)
    for data in glyphs:
        grid = to_grid(data, src_width, src_height, stride)
        grid = resize_grid(grid, src_width, src_height, width, height)
        minx, miny, maxx, maxy = bbox(grid, width, height)
        w = maxx - minx + 1
        h = maxy - miny + 1
        # bit-pack contiguously, MSB-first, row-major
        bits = []
        region = []
        for y in range(miny, maxy + 1):
            row = []
            for x in range(minx, maxx + 1):
                bits.append(grid[y][x])
                row.append(grid[y][x])
            region.append(row)
        packed = bytearray()
        acc = 0
        n = 0
        for bit in bits:
            acc = (acc << 1) | bit
            n += 1
            if n == 8:
                packed.append(acc); acc = 0; n = 0
        if n:
            packed.append(acc << (8 - n))
        trimmed.append((w, h, minx, miny, bytes(packed), region))

    # --- round-trip verify: decode packed -> compare to region ---
    for i, (w, h, minx, miny, packed, region) in enumerate(trimmed):
        idx = 0
        for y in range(h):
            for x in range(w):
                byte = packed[idx // 8]
                bit = (byte >> (7 - (idx % 8))) & 1
                assert bit == region[y][x], f"digit {i} mismatch at {x},{y}"
                idx += 1
    print("round-trip pixel check: OK (all 10 digits decode back exactly)")

    cell_w = max(t[0] for t in trimmed)
    x_advance = cell_w + CELL_GAP

    # build bitmap + glyph table
    bitmap = bytearray()
    glyph_rows = []
    offsets = []
    for (w, h, minx, miny, packed, region) in trimmed:
        offsets.append(len(bitmap))
        bitmap.extend(packed)
    for i, (w, h, minx, miny, packed, region) in enumerate(trimmed):
        x_offset = (cell_w - w) // 2          # centre the glyph in its tabular cell
        y_offset = miny - height              # reproduce original vertical position
        glyph_rows.append((offsets[i], w, h, x_advance, x_offset, y_offset))

    old_size = len(glyphs) * stride * src_height   # the fixed-cell source
    new_size = len(bitmap) + len(trimmed) * 7 + 8  # bitmap + glyph table + GFXfont
    print(f"digit widths: {[t[0] for t in trimmed]}  cell={cell_w} advance={x_advance}")
    print(f"bitmap bytes: {len(bitmap)}  (glyph table {len(trimmed)*7} + struct ~8)")
    print(f"flash: old kOledDigits {old_size} B -> GFXfont ~{new_size} B "
          f"(save ~{old_size - new_size} B, {100*(old_size-new_size)/old_size:.0f}%)")

    lines = []
    lines.append("#ifndef DRIVERS_OLED_FONT_BIGNUM_H")
    lines.append("#define DRIVERS_OLED_FONT_BIGNUM_H")
    lines.append("")
    lines.append("#include <Adafruit_GFX.h>  // GFXfont / GFXglyph")
    lines.append("")
    lines.append("// Auto-generated by tools/gen_oled_bignum.py from oled_font_digits.h.")
    lines.append("// Big tabular digits 0-9, trimmed + bit-packed, scaled proportionally")
    lines.append(f"// to a {width}x{height} cell (from {src_width}x{src_height}) so the bezel")
    lines.append("// over the glass cannot crop them. Render with oled.setFont(&OledBigNum).")
    lines.append("")
    body = ", ".join(f"0x{b:02X}" for b in bitmap)
    lines.append(f"static const uint8_t OledBigNumBitmaps[] = {{ {body} }};")
    lines.append("")
    lines.append("static const GFXglyph OledBigNumGlyphs[] = {")
    for d, (off, w, h, xa, xo, yo) in zip(DIGITS, glyph_rows):
        lines.append(f"    {{ {off:4d}, {w:2d}, {h:2d}, {xa:2d}, {xo:3d}, {yo:4d} }}, // '{d}'")
    lines.append("};")
    lines.append("")
    lines.append("static const GFXfont OledBigNum = {")
    lines.append("    (uint8_t *)OledBigNumBitmaps,")
    lines.append("    (GFXglyph *)OledBigNumGlyphs,")
    lines.append(f"    0x30, 0x39, {height}   // first '0', last '9', yAdvance")
    lines.append("};")
    lines.append("")
    lines.append("#endif // DRIVERS_OLED_FONT_BIGNUM_H")
    lines.append("")

    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    print(f"written {OUT}")


if __name__ == "__main__":
    main()
