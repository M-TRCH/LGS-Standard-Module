#ifndef CONFIG_H
#define CONFIG_H

/*  @file config.h
 *  @brief Firmware tunables and factory defaults.
 *
 *  Pure constants only — no code, no project includes. Pin mapping lives
 *  in board.h; compile-time device identity lives in version.h.
 */

// --- Factory defaults (loaded on factory reset / first boot) ---
#define DEFAULT_BAUD_RATE           9600
#define DEFAULT_IDENTIFIER          247     // Default Modbus ID (1-245, 246=SET_ID, 247=not set)
#define DEFAULT_LED_BRIGHTNESS      80      // percent (every preset)
#define DEFAULT_LED_MAX_ON_TIME     3600    // seconds (every preset)
#define DEFAULT_UNLOCK_DELAY_TIME   0       // milliseconds

// Factory palette for the 8 LED color presets ({brightness, R, G, B, maxOn}
// per preset) — the legacy per-light defaults, so an R5.0 board dropped into
// an R4.x site shows the colors that site expects.
#define DEFAULT_LED_PRESETS                                                  \
    {                                                                        \
        { DEFAULT_LED_BRIGHTNESS, 255,   0,   0, DEFAULT_LED_MAX_ON_TIME },  \
        { DEFAULT_LED_BRIGHTNESS,   0, 255,   0, DEFAULT_LED_MAX_ON_TIME },  \
        { DEFAULT_LED_BRIGHTNESS,   0,   0, 255, DEFAULT_LED_MAX_ON_TIME },  \
        { DEFAULT_LED_BRIGHTNESS, 255, 215,   0, DEFAULT_LED_MAX_ON_TIME },  \
        { DEFAULT_LED_BRIGHTNESS,   0, 255, 255, DEFAULT_LED_MAX_ON_TIME },  \
        { DEFAULT_LED_BRIGHTNESS, 255,   0, 255, DEFAULT_LED_MAX_ON_TIME },  \
        { DEFAULT_LED_BRIGHTNESS, 255,  60,   0, DEFAULT_LED_MAX_ON_TIME },  \
        { DEFAULT_LED_BRIGHTNESS, 255, 245, 120, DEFAULT_LED_MAX_ON_TIME },  \
    }

// --- Routine intervals ---
#define ROUTINE_BLINK_RUN_MS        1200    // RUN LED heartbeat interval
#define ROUTINE_BLINK_SET_ID_MS     800     // SET_ID mode blink interval
#define ROUTINE_SENSOR_READ_MS      1000    // per-sensor temperature refresh interval
#define DEMO_FRAME_MS               32      // demo rainbow animation frame time
#define DEMO_SWITCH_DEBOUNCE_MS     180     // demo mode counter button debounce

// --- Mode indication ---
#define SET_ID_BLINK_BLUE           204     // blue level while blinking in SET_ID mode
#define FACTORY_RESET_RED           204     // red level while in factory reset mode
#define FACTORY_RESET_HOLD_MS       5000    // solid-red warning time before reset applies

// --- SET_ID mode (manual ID assignment via the function switch) ---
#define SETID_SAVE_HOLD_MS          2000    // hold the switch this long to save & reboot
#define SETID_TAP_MIN_MS            30      // minimum press to count as a tap (debounce)
#define SETID_ID_MIN                1       // lowest ID settable by button (0 = broadcast, excluded)
#define SETID_ID_MAX                99      // highest ID settable by button (2-digit range)

// --- Latch safety limits ---
#define LATCH_PULSE_MS              300     // minimum unlock pulse (ms); extends while still locked
#define LATCH_MAX_UNLOCK_TIME       500     // hard cap on pulse width (ms) — solenoid spec max
#define LATCH_MIN_INTERVAL          2000    // minimum time between unlocks (ms), from pulse start
#define UNLOCK_DELAY_MAX_MS         8000    // clamp for the unlock-delay register (control table range)

// --- Fault indication ---
#define STORAGE_FAULT_BLINK_MS      300     // fast RUN-LED blink when the AT24 EEPROM is absent

// --- OTA over RS485 ---
#define OTA_SESSION_TIMEOUT_MS      30000   // no bus activity this long -> session fails out

// --- Diagnostics ---
#define TEMP_SENTINEL               0x8000  // reg 20/21 value when a sensor is faulted (-327.68C)
#define TEMP_FAIL_THRESHOLD         3       // consecutive read failures before the sentinel
#define DIAG_PUBLISH_INTERVAL_MS    1000    // uptime/health refresh cadence

// --- Identify (coil 509: find this unit in a wall of lockers) ---
#define IDENTIFY_DURATION_MS        5000    // how long the ring blinks
#define IDENTIFY_BLINK_MS           250     // blink half-period
#define IDENTIFY_WHITE_LEVEL        80      // white brightness (kept moderate: 16 px supply draw)

// --- Statistics persistence (AT24; settings blob owns bytes 0-95) ---
#define STATS_AT24_ADDR             128     // StatsBlob offset on the AT24C32D
#define STATS_PERSIST_INTERVAL_MS   3600000UL // hourly, plus a flush before every commanded reset

// --- Watchdog ---
// A single RTUServer.poll() can legitimately stall for hundreds of ms
// (long response flush + libmodbus byte timeouts under bus noise), so the
// timeout must comfortably exceed the worst chained poll, not the typical
// loop. The latch pulse clamp does not depend on this (hardware guard).
#define WATCHDOG_TIMEOUT_MS         4000

#endif // CONFIG_H
