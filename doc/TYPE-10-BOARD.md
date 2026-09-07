# Type 10 (STANDARD) — what the board does, and what it does not

The reference for a STANDARD module: the R5.1 board built **without its
bottom layer**, with the LED-8-Index mask plugged into CN4 instead of the
ring. This is the module the LGS type 56 cabinet is made of.

Everything below was read off a live board on 2026-09-07 (id 247, fw 30500,
hw 510) unless marked otherwise. Where a type 20 (NARCOTIC, ring + OLED)
board behaves differently, that is called out — one firmware image serves
both and decides which it is at boot.

---

## 1. What is fitted, and what is deliberately absent

| Part | Type 10 | Consequence on the wire |
|---|---|---|
| LED-8-Index mask, 8× SK6812MINI RGBW | **fitted** (CN4) | the display; see §2 |
| 16-pixel WS2812B ring | **not fitted** (bottom layer unpopulated) | — |
| OLED 0.96" | **not fitted** | `reg 9` bit1 always **0**; `reg 60` inert |
| STS40 room + board temperature sensors | **NEITHER IS FITTED** | `reg 20` and `reg 21` read **0x8000** (32768) forever; `reg 9` bits 2 and 3 always **0** |
| AT24C32D EEPROM | fitted | `reg 9` bit0 = 1; settings and statistics persist |
| INA180A4 current sense | fitted | `reg 22` in mA (reads ~5 mA with the mask dark) |
| Servo latch | **per cabinet** — optional | `reg 40`/`reg 41` and the latch coils only mean something when one is wired |
| SW1 / SW3 function switches | on the board, **rear side** | technician-only (mode select at boot). NOT a user control — see §6 |

> ### The one thing a monitoring server must be told
>
> **`reg 20` = `reg 21` = 0x8000 and `reg 9` bits 1, 2, 3 = 0 is a HEALTHY
> type-10 module.** 0x8000 is the firmware's "sensor did not answer" sentinel
> and those health bits report parts this variant does not carry. A server
> that alarms on them will alarm on all 56 modules, forever, on day one.
>
> On a type-10 board only **bit0 (AT24 ok)** is a real health signal, plus
> **bit4 (latch locked)** where a latch is fitted.

A healthy board, freshly booted, reads:

```
reg  0 device type   = 10          reg  9 health      = 1     (bit0 only)
reg  1 firmware      = 30500       reg 10 func mode   = 0     (RUN)
reg  2 hardware      = 510         reg 11 active      = 0
reg  3 baud          = 9600        reg 20/21 temps    = 32768 (absent — normal)
reg  4 slave id      = 247         reg 22 current     = ~5 mA
reg  7 boot count    = n           reg 61 lit windows = 0
```

---

## 2. The display: eight INDEPENDENT windows

The mask is eight separately addressable windows in a 2×4 grid, and
**window n means person n**: one slot can carry up to eight people's
medications at once, so any subset of the eight may be lit simultaneously.
This is the R4.x fleet's behaviour, restored for the mask in **fw 3.5.0**.

> **fw ≤ 3.4.0 on a mask board is RADIO** — one window at a time, each new
> one closing the last. A type-56 cabinet must not ship on it.

A ring board (type 20) keeps radio semantics on the same firmware, because a
ring can only show one colour at a time.

Default colours, unchanged from R4.x, one per window:

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|
| red | green | blue | gold | cyan | magenta | orange | warm white |
| 255,0,0 | 0,255,0 | 0,0,255 | 255,215,0 | 0,255,255 | 255,0,255 | 255,60,0 | 255,245,120 |

All eight default to brightness 80 and max-on-time 3600 s.

Window numbering is what a person reads off the front, not the data-chain
order — the chain snakes, and `drivers/led_mask.cpp` holds the mapping with
its derivation from the PCB artwork. Windows 1-4 sit in one row and 5-8 in
the other; **confirm that visually on the first assembled cabinet**, since
the mapping is derived from the artwork rather than witnessed.

Two rendering details worth knowing:

* A neutral colour (R = G = B) is routed to the RGBW part's dedicated white
  die instead of being mixed. Preset 8's default (255,245,120) is *not*
  neutral, so it renders as mixed RGB. To get true white, write equal
  values — e.g. 200/200/200.
* Colour and brightness changes do not repaint a window that is already lit;
  they take effect at its next on-edge. Same rule the ring has always had.

---

## 3. Registers

| Addr | Meaning | Type 10 |
|---|---|---|
| 0-2 | device type / firmware / hardware | 10 / ≥30500 / 510 |
| 3-4 | baud / slave id | 9600 default; id 247 until commissioned |
| 5-6 | uptime (u32) | |
| 7 | boot count | survives everything but a factory reset |
| 8 | last reset cause (bitfield) | bit0 IWDG is the one that matters |
| 9 | health (bitfield) | **only bit0 and bit4 are meaningful here** — see §1 |
| 10 | function mode | 0 RUN / 1 DEMO / 2 SET_ID / 3 FACTORY_RESET |
| 11 | active preset | last window commanded on **while still lit**, else the lowest lit, else 0 |
| 12-17 | 96-bit silicon UID | matches the commissioning log |
| 18-19 | button presses / held | **dormant in the field** (§6) |
| 20-21 | room / board temperature | **always 0x8000 — not fitted** |
| 22 | input current, mA | |
| 40 | seconds since last unlock | latch cabinets only |
| 41 | latch locked | latch cabinets only |
| **60** | number for the display | **inert — no OLED** |
| **61** | **lit-window bitmask, bit n-1 = window n** | **the multi-window truth; fw ≥ 3.5.0** |
| 80 | delay before unlock, ms | |
| 110-184 | per-window brightness / R / G / B / max-on-time | five registers per window at `100 + 10n` |
| 190 / 194 | global brightness / global max-on-time | fan out to all eight blocks |
| 200-201 | total on-count / runtime | |
| 210-281 | per-window on-count / runtime | genuinely per window |
| 282-389 | OTA | §5 |
| 400-451 | statistics v2 (u32) | latch fires, operating seconds, **IWDG count** |

**`reg 11` versus `reg 61`.** `reg 11` carries one number and cannot express
"windows 2, 5 and 7". It is kept for masters that read a single value, and on
single-window use it reads exactly as it always did. Anything that needs the
truth reads `reg 61`.

---

## 4. Coils

| Coils | Use on type 10 |
|---|---|
| **1001-1008** enable window 1-8 | **the primary command.** Independent: light any subset, clear one without touching its siblings |
| 1021-1028 window + latch | latch cabinets: lights the window *and* unlocks, in one command |
| 1019 / 1020 latch alone | 1020 is sense-aware (only fires while the latch reads locked); 1019 always fires |
| 1011-1018, 1031-1038 (+ display) | work, and keep correct window semantics, but the display half does nothing here |
| 509 identify | whole mask blinks white ~5 s, then restores the lit set untouched |
| 510 clear statistics | zeroes usage counters (not the boot count) |
| 511 all off | every window dark, every mirror coil cleared — the recovery command when a master has lost track |
| 500-504 | factory reset / persist to EEPROM / software reset |
| 505-508 | OTA — §5 |

Latch safety limits apply to every command that unlocks: pulse ≤ 500 ms
(enforced again by a hardware timer), and **at least 2000 ms between
unlocks**. A request inside that window is refused and its coil cleared
immediately, so it does not retry into a surprise unlock later.

---

## 5. OTA

Works on a mask board — verified end to end 2026-09-07, both directions,
image intact and functional afterwards.

* Image must be built for the app slot (offset `0x1000`) and be **≤ 61,440
  bytes**. v3.5.0-dev is 60,932 — only **508 bytes of headroom**, so the next
  feature needs a diet pass first.
* Roughly **120 s per module at 9600 baud** (477 chunks of 128 B).
* `tools/ota_sender.py` paces broadcasts by the frame's wire time. Do not
  lower `--gap` below the default: a flat gap that is faster than the wire
  loses most of the stream when the path runs through a USB→RS485 bridge.

---

## 6. Pick confirmation does NOT happen on the board

A type-10 cabinet has **no user-facing switch**. The pharmacist confirms on a
tablet and the server clears the window. The module is a display.

* `reg 18`/`reg 19` and the press-acknowledge blink stay dormant in the
  field. SW1 and SW3 sit on the rear of the board and are service controls
  (boot-time mode select), not a pick button.
* The latch, where fitted, is a cabinet mechanism — not the confirmation
  path.
* **The per-window max-on-time is therefore the only thing that ever puts a
  forgotten window out.** It defaults to 3600 s. Set it deliberately at
  commissioning to whatever "nobody is coming back for this" means on site.

---

## 7. What a server must do differently from a type-64 cabinet

1. **Drive `1001-1008`, and expect several lit at once.** Read `reg 61`, not
   `reg 11`, to know the state.
2. **Do not alarm on `reg 20`/`reg 21` = 0x8000 or on health bits 1-3.**
   That is a correctly built type-10 module.
3. **Ignore `reg 60`.** The "register 60 now accepts 0-999" line from the
   type-64 contract does not apply — there is no display to write to.
4. **Clear windows explicitly.** Nothing on the module does it for you until
   max-on-time expires.
5. Everything else carries over from the type-64 contract: **read timeout
   ≥ 4 s and retry**, and one Modbus master on the RS485 bus at a time.

---

## 8. Provenance

Bench-verified on a type-10 board with a real mask, 2026-09-07:
`tools/window_engine_test.py --deep` passes **59/59** — eight windows lit
simultaneously, per-window statistics isolated and surviving a reboot,
per-window max-on-time expiry, all four coil families keeping siblings lit,
identify restoring the set, and a 60-toggle churn leaving the bitmask exact.

Still outstanding before v3.5.0 ships: the **ring regression** on a type 20
board, since `app/led_control.cpp` serves both. And no type-10 hardware has
been soaked yet — all long-run evidence in this project comes from ring
boards.
