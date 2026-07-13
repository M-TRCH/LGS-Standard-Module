#ifndef SVC_MODBUS_MAP_H
#define SVC_MODBUS_MAP_H

#include <stdint.h>

/*  @file svc/modbus_map.h
 *  @brief Single source of truth for the R5.0 Modbus address map.
 *
 *  Reference: doc/LGS-Control-Table.md (R5.0 column). This is the ONLY
 *  file allowed to spell a raw Modbus address. Boards below R5.0 use a
 *  wider legacy map that lives in the documentation only.
 *
 *  Access notes per address are in the control table; (F) = persisted
 *  to the external EEPROM via the Write-to-Flash coil (503).
 */

// --- Device group (holding registers, read-only 0-2, R/W(F) 3-4) ---
constexpr uint16_t MB_REG_DEVICE_TYPE       = 0;    // 10/20/30/40, compile-time
constexpr uint16_t MB_REG_FW_VERSION        = 1;    // ddmmy, compile-time
constexpr uint16_t MB_REG_HW_VERSION        = 2;    // mnp, compile-time (500 = R5.0)
constexpr uint16_t MB_REG_BAUD_RATE         = 3;    // bps, whitelist, (F)
constexpr uint16_t MB_REG_IDENTIFIER        = 4;    // slave ID 1-245, 246 special, (F)

// --- Sensor group (holding registers, read-only) ---
constexpr uint16_t MB_REG_ROOM_TEMP         = 20;   // STS40-CD1B, deg C x100
constexpr uint16_t MB_REG_BOARD_TEMP        = 21;   // STS40-AD1B, deg C x100
constexpr uint16_t MB_REG_TIME_AFTER_UNLOCK = 40;   // seconds since last unlock

// --- Configuration group (holding registers) ---
constexpr uint16_t MB_REG_SET_NUM_DISPLAY   = 60;   // 0-9999 shown on OLED
constexpr uint16_t MB_REG_UNLOCK_DELAY      = 80;   // milliseconds 0-8000, (F)
constexpr uint16_t MB_REG_LED_1_BRIGHTNESS  = 110;  // 0-100, (F)
constexpr uint16_t MB_REG_LED_1_RED         = 111;  // 0-255, (F)
constexpr uint16_t MB_REG_LED_1_GREEN       = 112;  // 0-255, (F)
constexpr uint16_t MB_REG_LED_1_BLUE        = 113;  // 0-255, (F)
constexpr uint16_t MB_REG_LED_1_MAX_ON_TIME = 114;  // seconds, 0 = unlimited, (F)
constexpr uint16_t MB_REG_GLOBAL_BRIGHTNESS = 190;  // fan-out write to LED brightness
constexpr uint16_t MB_REG_GLOBAL_MAX_ON_TIME= 194;  // fan-out write to LED max-on-time

// --- Statistics group (holding registers, read-only) ---
constexpr uint16_t MB_REG_TOTAL_LED_ON_CNT  = 200;
constexpr uint16_t MB_REG_TOTAL_LED_ON_TIME = 201;  // seconds
constexpr uint16_t MB_REG_LED_1_ON_COUNTER  = 210;
constexpr uint16_t MB_REG_LED_1_ON_TIME     = 211;  // seconds

// --- Operation group (coils) ---
constexpr uint16_t MB_COIL_FACTORY_RESET                 = 500;
constexpr uint16_t MB_COIL_APPLY_FACTORY_RESET_EXCEPT_ID = 501;
constexpr uint16_t MB_COIL_APPLY_FACTORY_RESET_ALL_DATA  = 502;
constexpr uint16_t MB_COIL_WRITE_TO_EEPROM               = 503;
constexpr uint16_t MB_COIL_SOFTWARE_RESET                = 504;

// --- Control group (coils) ---
constexpr uint16_t MB_COIL_LED_1_ENABLE     = 1001; // full 16-pixel ring
constexpr uint16_t MB_COIL_DISPLAY_ENABLE   = 1010; // reserved: display feature
constexpr uint16_t MB_COIL_LED_1_DISPLAY    = 1011; // reserved: display feature
constexpr uint16_t MB_COIL_LATCH_TRIGGER    = 1020;
constexpr uint16_t MB_COIL_LED_1_LATCH      = 1021; // LED on + latch pulse in one command

// --- Reserved for future features (documented only, not yet implemented) ---
// Servo control (PC6/PC7): addresses to be assigned when the feature lands.

// Freeze the wire contract: these values are what deployed masters and the
// control table agree on. Any edit here must be a deliberate map change.
static_assert(MB_REG_IDENTIFIER == 4,            "wire contract");
static_assert(MB_REG_ROOM_TEMP == 20,            "wire contract");
static_assert(MB_REG_BOARD_TEMP == 21,           "wire contract");
static_assert(MB_REG_TIME_AFTER_UNLOCK == 40,    "wire contract");
static_assert(MB_REG_UNLOCK_DELAY == 80,         "wire contract");
static_assert(MB_REG_LED_1_BRIGHTNESS == 110,    "wire contract");
static_assert(MB_REG_LED_1_MAX_ON_TIME == 114,   "wire contract");
static_assert(MB_REG_GLOBAL_BRIGHTNESS == 190,   "wire contract");
static_assert(MB_REG_TOTAL_LED_ON_CNT == 200,    "wire contract");
static_assert(MB_REG_LED_1_ON_COUNTER == 210,    "wire contract");
static_assert(MB_COIL_WRITE_TO_EEPROM == 503,    "wire contract");
static_assert(MB_COIL_LED_1_ENABLE == 1001,      "wire contract");
static_assert(MB_COIL_LATCH_TRIGGER == 1020,     "wire contract");
static_assert(MB_COIL_LED_1_LATCH == 1021,       "wire contract");

#endif // SVC_MODBUS_MAP_H
