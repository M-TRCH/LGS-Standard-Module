# libmodbus "confirmation to ignore" parity fix — test record

Status: **lab-tested, not released.** Built as FW 30599 (`-DFW_VERSION=30599`, `version.h` untouched, still 30504)
so a patched module cannot be mistaken for a released one. A real version number is a separate decision.

## The bug
`_modbus_rtu_receive()` in the bundled ArduinoModbus 1.0.9 libmodbus sets `confirmation_to_ignore` after a frame
addressed to another slave and discards the NEXT frame whatever it is (it assumes every request is followed by a
reply). On our multi-drop channels any unanswered request (dead id, stalled module, a frame the hub cut) leaves the
other modules one frame out of phase: each eats its own next request as the "reply" and stays silent. The state is
stable until a CRC error or a power cycle changes the parity. Own FC05/06 writes swallowed this way are lost.

## The fix
`LgsRtuServer::judgeEveryFrame()` (`src/svc/modbus_server.cpp`) clears `confirmation_to_ignore` after every
`poll()`. `modbusServerTick` already hands libmodbus exactly one complete frame per call (frame-gap gate), and a
frame for another slave — a request or another module's reply — is rejected by its address anyway, so the flag
can only do harm. Cost: +16 B flash (OTA image 61,304 B, 136 B under the 61,440 B cap), RAM unchanged (4,988 B).

## Evidence (2026-10-23 .. 24, bench cabinets, gateway 1.13.0-rc2 with `RESYNC 0` so the gateway cannot mask it)
1. **Bench, Std-08, dead id 15 as the unanswered request** (`parity_bench2.py`, one TCP connection, `EXC 1`).
   id 11 = 3.5.99, id 12 = 3.5.4 control, same channel:

   | phase | id 11 (patched) | id 12 (control) |
   |---|---|---|
   | P2 read right after the dead id | 0 miss / 40 | 20 miss / 40 |
   | P3 `11,15,12` x30 | 0 / 30 | 30 / 30 |
   | P4 `11,12,15` x30 | 0 / 30 | 29 / 30 |
   | P5 `11,12` x30 | 0 / 30 | 1 / 30 |

   The unpatched baseline (3.5.4 both, gateway 1.12.4/rc2 without heal) locked both modules for good (P5 0/50, 0/50).
2. **Pair probe, 4 h, normal traffic, gateway heal ON:** 266,185 reads, 0 misses, 8 hour marks clean (patched id
   did not get worse).
3. **12-cabinet A/B soak, 16.7 h, pharmacy 43,200 picks/day:** 24 patched modules (2 per cabinet) vs 24 controls on
   the same hub channel, patched ids chosen from the WORST history (bias against the patch). "Slow" events (first
   try > 400 ms, counted per id):

   | | patched | control |
   |---|---|---|
   | Ref-12 (row 2) | 51 | 2,336 |
   | the other 11 cabinets | **0** | 4,287 |

   No gateway rebooted (`sys.up` > 139,000 s), `rtu.heal=0` throughout. Std-04 was clean for both groups, so the
   difference comes from the cabinets that show the symptom.

## What it does not fix
- **Ref-12 row 2 still has 51 events on patched ids and the gateway's first `rtu.bad_crc` (=3).** A part of that row
  is electrical (wiring / termination / cold), not framing. Unpatched ids of that row (21, 23, 24, 27) were not measured.
- A module that is genuinely stalled (hourly AT24 stats flush, 25–60 ms) still answers late; the gateway's
  `rs485.t2_ms`/reply window absorbs that, not this patch.

## Roll-out checklist (not done)
- Decide the version number, update `FIRMWARE_VERSION_NOTE.md`, build the factory image.
- Keep the 3.5.4 image for rollback; OTA ~479 chunks per channel group.
- 24 bench modules currently run 30599 (see the cabinet plan in the gateway test records).
