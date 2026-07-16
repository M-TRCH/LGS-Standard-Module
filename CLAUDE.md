# LGS Standard Module — Firmware

Locker/cabinet control module. Board **R5.0** (STM32G070CBT6, 64 MHz, 128 KB flash / 36 KB RAM),
Arduino framework + PlatformIO. Peripherals: WS2812B 16-pixel LED ring, electronic latch
(MOSFET + sense pin), Modbus RTU slave over RS485, 0.96" SSD1306 OLED, two STS40 temperature
sensors, external AT24C32D EEPROM, function switch, 2 reserved servo outputs.

> Firmware for boards **older than R5.0** (R4.x / STM32F103) lives in `assets/*.bin` and is NOT
> built from this tree. This source targets R5.0 only.

## Build / flash

PlatformIO is **not on PATH** — call the full path:

```bash
"$USERPROFILE/.platformio/penv/Scripts/platformio.exe" run          # build app (env: LGS_STM32G070CBT6)
"$USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -t upload # flash app via ST-Link
"$USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e LGS_BOOT -t upload  # flash bootloader (once per board)
"$USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -t clean
```

**Flash layout (OTA)**: `[boot 4KB @0x08000000][app 62KB @0x08001000][staging 62KB]`
(`include/flash_layout.h` = SSOT shared with `src/boot/`). The app links at offset 0x1000
with a hard `board_upload.maximum_size` cap — **an app over 61,440 B fails at link time**
(that's the OTA staging limit). Field updates go over RS485 with
`tools/ota_sender.py -p COMx --ids <id,...> -f firmware.bin` (Modbus broadcast, per-device
verify + chunk repair); ST-Link is only needed once per board to install boot + offset app.

There is no host test suite; verification is `pio run` (build) plus on-hardware checks using the
Python scripts in `tools/` (`test_modbus_rtu.py` sweep tester, `ota_sender.py`). **Record
flash/RAM every build** against the budget table in `doc/ARCHITECTURE.md`.

## Architecture (see `doc/ARCHITECTURE.md` for the full version)

Strict downward-only layers; nothing includes upward:

```
L0 constants  include/{version.h, board.h, config.h}    — pure constants, no project includes
L1 util       src/util/periodic_timer.h                 — Arduino.h only
L2 drivers/   one device per module, object file-static, functions only
              board_io · rs485_port · led_ring · oled · temp_sensor · eeprom_at24 ·
              flash_stage (OTA staging) · servo_out
              → include L0 + vendor lib ONLY (never svc/ or app/)
L3 svc/       settings · modbus_map.h (address SSOT) · modbus_server (persist + watch tables)
L4 app/       app (boot + tick pipeline) · modes · led_control (8 presets) · latch_control ·
              ops · display_control · ota_control · servo_control (stub)
main.cpp → app.h only
src/boot/     bare-metal bootloader (env LGS_BOOT, framework=cmsis) — outside the app layers
```

`appRun()` is a fixed tick pipeline — **order is a safety invariant**:
`modbusServerTick()` → sample `now = millis()` → `latchControlTick(now)` → `ledControlTick(now)` → mode.
Sample `now` AFTER the Modbus tick (handlers stamp state with a fresh millis(); an earlier sample
underflows and instantly expires delays / max-on-time).

## Conventions & hard rules

- **No `String`, heap, or `float` in runtime paths** (flash/OTA budget). OLED uses `char buf` +
  `snprintf` (no `%f`); LED stats accumulate `uint32` ms. Build uses `-flto`.
- **Non-blocking runtime.** The only intentional `delay()` is the boot-time function-switch hold in
  `app/modes.cpp`. Latch/factory-reset/etc. are state machines. Don't reintroduce blocking calls.
- **Raw Modbus addresses live ONLY in `src/svc/modbus_map.h`** (with `static_assert`s freezing the
  wire contract). The supported R5.0 address set is documented in `doc/LGS-Control-Table.md` (R5.0
  column); legacy R4.x commands are documentation-only.
- **Latch safety (SAFETY-CRITICAL):** pulse ≤ 500 ms, ≥ 2000 ms between pulses. `latch_control` is an
  IDLE→DELAY→PULSE→COOLDOWN FSM with a TIM7 hardware guard forcing the MOSFET low at the clamped
  width regardless of loop stalls; every reset path goes through `opsSystemReset()` which drives the
  MOSFET low first.
- **Config persists on the AT24C32D** (I2C1 @0x50), a versioned magic+CRC blob (`svc/settings`), not
  MCU flash. Routine saves rewrite payload+CRC only (never the magic) so a torn write degrades to a
  bad CRC, not a lost magic.
- **Device identity is compile-time** (`include/version.h`: `DEVICE_TYPE`/`FW_VERSION`/`HW_VERSION`,
  overridable via `-D`). Reg 0–2 report these — never store them in EEPROM.

## Add a feature (recipe)

New Modbus command = add the address to `svc/modbus_map.h` (+`static_assert`) and a row to
`doc/LGS-Control-Table.md`, register one handler via `mbRegisterHandler(kind, addr, cb)` in the
owning app module, and (if persisted) add a `Settings` field + a `kPersistRows` entry. See the
Cookbook in `doc/ARCHITECTURE.md`.

## Docs

- `doc/ARCHITECTURE.md` — layers, tick invariant, latch FSM, settings blob, OLED per-mode behavior,
  flash/OTA budget, verification gates, cookbook
- `doc/LGS-Control-Table.md` — Modbus map with the R5.0 support column
- `doc/FIRMWARE_VERSION_NOTE.md` / `doc/HARDWARE_VERSION_NOTE.md` — release + board history
