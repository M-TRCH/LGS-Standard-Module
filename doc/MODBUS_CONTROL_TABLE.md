# LGS Modbus Control Table

Modbus RTU register / coil map for the LGS (Light Guiding Shelf) Standard Module.
Generated from `LGS-Control-Table.xlsx`. These addresses are the single source
of truth and are mirrored in code by [`include/modbus_map.h`](../include/modbus_map.h).

- **Access:** `R` = read-only, `R/W` = read/write, `R/W(F)` = read/write + stored in flash
- **Holding Registers:** function code 03 (read) / 06 / 16 (write), 2 bytes each
- **Coils:** function code 01 (read) / 05 / 15 (write), 1 bit each

---

## Device (Holding Registers)

| Addr | Name | Access | Init | Range | Unit |
|-----:|------|--------|-----:|-------|------|
| 0 | Device Type | R | - | 10:STANDARD, 20:NARCOTIC, 30:LITE, 40:DELIVERY | - |
| 1 | Firmware Version | R | - | - | - |
| 2 | Hardware Version | R | - | - | - |
| 3 | Baud Rate | R/W(F) | 9600 | - | bps |
| 4 | Modbus Slave ID | R/W(F) | 247 | 1-245, 246:SPECIFIC | - |

## Sensor (Holding Registers)

| Addr | Name | Access | Range | Unit |
|-----:|------|--------|-------|------|
| 20 | Built-in Temperature | R | 100-9000 | °C ×100 |
| 40 | Time after unlocking | R | 0-65535 | sec |

## Configuration (Holding Registers)

| Addr | Name | Access | Init | Range | Unit |
|-----:|------|--------|-----:|-------|------|
| 60 | Set number on display | R/W | 0 | 0-9999 | - |
| 80 | Unlock delay time | R/W(F) | 0 | 0-65535 | ms |
| 81 | LED count per strip | R/W(F) | 1 | - | - |
| 190 | Global Brightness | R/W | 80 | 0-100 | - |
| 194 | Global Max On Time Limit | R/W | 3600 | 0-65535 | sec |

### Per-channel configuration block (stride = 10)

Channel `N` (1-8) base address = `110 + (N-1) × 10`.

| Offset | Field | Access | Init (Ch.1) | Range | Unit |
|-------:|-------|--------|------------:|-------|------|
| +0 | Light N Brightness | R/W(F) | 80 | 0-100 | - |
| +1 | Light N Red Value | R/W(F) | 255 | 0-255 | - |
| +2 | Light N Green Value | R/W(F) | 0 | 0-255 | - |
| +3 | Light N Blue Value | R/W(F) | 0 | 0-255 | - |
| +4 | Light N Max On Time Limit | R/W(F) | 3600 | 0-65535 | sec |

Resulting base addresses: Ch.1=110, Ch.2=120, Ch.3=130, Ch.4=140, Ch.5=150,
Ch.6=160, Ch.7=170, Ch.8=180.

## Statistics (Holding Registers, read-only)

| Addr | Name | Range | Unit |
|-----:|------|-------|------|
| 200 | Total Light On Count | 0-65535 | times |
| 201 | Total Light Runtime | 0-65535 | sec |
| 290 | Display On Count | 0-65535 | times |
| 291 | Display Runtime | 0-65535 | sec |

### Per-channel statistics block (stride = 10)

Channel `N` (1-8) base address = `210 + (N-1) × 10`.

| Offset | Field | Range | Unit |
|-------:|-------|-------|------|
| +0 | Light N On Count | 0-65535 | times |
| +1 | Light N Runtime | 0-65535 | sec |

Resulting base addresses: Ch.1=210, Ch.2=220, ... Ch.8=280.

## Operation (Coils)

| Addr | Name | Access |
|-----:|------|--------|
| 500 | Factory Reset | R/W |
| 501 | Apply Factory Reset (Except ID) | R/W |
| 502 | Apply Factory Reset (All Data) | R/W |
| 503 | Write data to Flash Memory | R/W |
| 504 | Software Reset | R/W |

## Control (Coils)

| Addr | Name | Access |
|-----:|------|--------|
| 1001-1008 | Enable Light 1-8 | R/W |
| 1010 | Enable Display | R/W |
| 1011-1018 | Enable Light 1-8 + Display | R/W |
| 1020 | Trigger Latch | R/W |
| 1021-1028 | Trigger Light 1-8 + Latch | R/W |

Channel `N` (1-8) coil addresses:
`Enable = 1000 + N`, `Enable+Display = 1010 + N`, `Trigger+Latch = 1020 + N`.
