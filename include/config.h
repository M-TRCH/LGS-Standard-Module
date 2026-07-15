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
#define DEFAULT_LED_BRIGHTNESS      80      // percent
#define DEFAULT_LED_R               255
#define DEFAULT_LED_G               0
#define DEFAULT_LED_B               0
#define DEFAULT_LED_MAX_ON_TIME     3600    // seconds
#define DEFAULT_UNLOCK_DELAY_TIME   0       // milliseconds

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

// --- Watchdog ---
// A single RTUServer.poll() can legitimately stall for hundreds of ms
// (long response flush + libmodbus byte timeouts under bus noise), so the
// timeout must comfortably exceed the worst chained poll, not the typical
// loop. The latch pulse clamp does not depend on this (hardware guard).
#define WATCHDOG_TIMEOUT_MS         4000

#endif // CONFIG_H
