#ifndef MODBUS_MAP_H
#define MODBUS_MAP_H

#include <Arduino.h>

/*
 * modbus_map.h - Modbus register / coil address map
 * -------------------------------------------------
 * Generated from "LGS Control Table" (LGS-Control-Table.xlsx).
 *
 * Addresses are kept 100% backward compatible with the 8-channel map that is
 * already deployed in the field. Instead of scattering magic numbers such as
 * `MB_REG_LED_1_RED + i*10` throughout the code, use the strongly-typed
 * enumerations and the small inline helpers at the bottom of this file.
 *
 * Groups:
 *   Device        Holding Registers  0   .. 4
 *   Sensor        Holding Registers  20  .. 40
 *   Configuration Holding Registers  60  .. 194
 *   Statistics    Holding Registers  200 .. 291
 *   Operation     Coils              500 .. 504
 *   Control       Coils              1001 .. 1028
 */

// ---------------------------------------------------------------------------
// Holding registers (Function code 03/06/16, 2 bytes each)
// ---------------------------------------------------------------------------
enum ModbusReg : uint16_t
{
    // Device group (read-only unless noted)
    MB_REG_DEVICE_TYPE          = 0,    // 10=standard, 20=narcotic, 30=lite, 40=delivery
    MB_REG_FW_VERSION           = 1,
    MB_REG_HW_VERSION           = 2,
    MB_REG_BAUD_RATE            = 3,    // R/W(F)
    MB_REG_IDENTIFIER           = 4,    // R/W(F), 1-246, 247=not set

    // Sensor group (read-only)
    MB_REG_BUILT_IN_TEMP        = 20,   // Temperature x100 (e.g. 2534 = 25.34 C)
    MB_REG_TIME_AFTER_UNLOCK    = 40,   // Seconds since last latch unlock

    // Configuration group
    MB_REG_SET_NUM_DISPLAY      = 60,   // R/W  Number shown on the display
    MB_REG_UNLOCK_DELAY         = 80,   // R/W(F) Unlock delay time
    MB_REG_LED_NUM_PER_STRIP    = 81,   // R/W(F) LEDs per strip

    // Per-channel LED configuration block (stride = MB_LED_CFG_STRIDE)
    MB_REG_LED_1_BRIGHTNESS     = 110,  // Channel 1 config base
    MB_REG_LED_1_RED            = 111,
    MB_REG_LED_1_GREEN          = 112,
    MB_REG_LED_1_BLUE           = 113,
    MB_REG_LED_1_MAX_ON_TIME    = 114,

    MB_REG_GLOBAL_BRIGHTNESS    = 190,  // R/W  Applies to every channel at once
    MB_REG_GLOBAL_MAX_ON_TIME   = 194,  // R/W  Applies to every channel at once

    // Statistics group (read-only)
    MB_REG_TOTAL_LED_ON_CNT     = 200,
    MB_REG_TOTAL_LED_ON_TIME    = 201,
    MB_REG_LED_1_ON_COUNTER     = 210,  // Channel 1 status base (stride = MB_LED_STATUS_STRIDE)
    MB_REG_LED_1_ON_TIME        = 211,
    MB_REG_DISPLAY_ON_CNT       = 290,
    MB_REG_DISPLAY_ON_TIME      = 291,
};

// ---------------------------------------------------------------------------
// Coils (Function code 01/05/15, 1 bit each)
// ---------------------------------------------------------------------------
enum ModbusCoil : uint16_t
{
    // Operation group
    MB_COIL_FACTORY_RESET                   = 500,
    MB_COIL_APPLY_FACTORY_RESET_EXCEPT_ID   = 501,
    MB_COIL_APPLY_FACTORY_RESET_ALL_DATA    = 502,
    MB_COIL_WRITE_TO_EEPROM                 = 503,
    MB_COIL_SOFTWARE_RESET                  = 504,

    // Control group
    MB_COIL_LED_1_ENABLE        = 1001,  // Channels 1..8 => 1001..1008
    MB_COIL_DISPLAY_ENABLE      = 1010,
    MB_COIL_LED_1_DISPLAY       = 1011,  // Enable channel N + display => 1011..1018
    MB_COIL_LATCH_TRIGGER       = 1020,
    MB_COIL_LED_1_LATCH         = 1021,  // Enable channel N + latch => 1021..1028
};

// ---------------------------------------------------------------------------
// Per-channel addressing helpers (eliminate `+ i*10` magic numbers)
// ---------------------------------------------------------------------------
constexpr uint16_t MB_LED_CFG_STRIDE    = 10;   // Registers between channel config blocks
constexpr uint16_t MB_LED_STATUS_STRIDE = 10;   // Registers between channel status blocks

// Offset of each field inside a channel's configuration block
enum LedCfgField : uint16_t
{
    LED_CFG_BRIGHTNESS   = 0,
    LED_CFG_RED          = 1,
    LED_CFG_GREEN        = 2,
    LED_CFG_BLUE         = 3,
    LED_CFG_MAX_ON_TIME  = 4,
};

// Offset of each field inside a channel's status block
enum LedStatusField : uint16_t
{
    LED_STAT_ON_COUNTER  = 0,
    LED_STAT_ON_TIME     = 1,
};

// Holding register address of a channel configuration field (ledIndex 0-based)
inline uint16_t mbLedCfg(uint8_t ledIndex, LedCfgField field)
{
    return MB_REG_LED_1_BRIGHTNESS + ledIndex * MB_LED_CFG_STRIDE + field;
}

// Holding register address of a channel status field (ledIndex 0-based)
inline uint16_t mbLedStatus(uint8_t ledIndex, LedStatusField field)
{
    return MB_REG_LED_1_ON_COUNTER + ledIndex * MB_LED_STATUS_STRIDE + field;
}

// Control coil addresses for a given channel (ledIndex 0-based)
inline uint16_t mbLedEnableCoil(uint8_t ledIndex)  { return MB_COIL_LED_1_ENABLE  + ledIndex; }
inline uint16_t mbLedDisplayCoil(uint8_t ledIndex) { return MB_COIL_LED_1_DISPLAY + ledIndex; }
inline uint16_t mbLedLatchCoil(uint8_t ledIndex)   { return MB_COIL_LED_1_LATCH   + ledIndex; }

#endif // MODBUS_MAP_H
