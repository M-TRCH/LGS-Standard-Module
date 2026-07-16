#ifndef SVC_SETTINGS_H
#define SVC_SETTINGS_H

#include <Arduino.h>

/*  @file svc/settings.h
 *  @brief Persistent configuration, stored on the external AT24C32D EEPROM.
 *
 *  The blob is versioned (magic + schema version + CRC16). Loading follows
 *  a strict order:
 *    1. valid v2 blob on the AT24         -> use it
 *    2. valid v1 blob                     -> one-time in-place migration
 *       (v1 fields become preset 1; presets 2-8 = factory palette)
 *    3. no/foreign magic                  -> format with factory defaults
 *    4. valid magic but bad CRC (torn)    -> defaults, salvaging a plausible
 *       identifier
 *    5. AT24 absent                       -> defaults in RAM, nothing persists
 *       (settingsStorageOk() returns false)
 */

// One LED color preset (regs 100+10n .. 104+10n). All fields uint16_t —
// the Modbus persist plumbing addresses them as a flat uint16 array.
struct LedPreset
{
    uint16_t brightness;        // 0-100
    uint16_t r;                 // 0-255
    uint16_t g;                 // 0-255
    uint16_t b;                 // 0-255
    uint16_t maxOnTimeS;        // seconds, 0 = unlimited
};

// Values persisted for the R/W(F) Modbus registers (schema v2).
// identifier MUST stay the first field: the torn-blob recovery salvages it
// by offset 0, across schema versions.
struct Settings
{
    uint16_t identifier;        // Modbus slave ID (reg 4)
    uint16_t baudRate;          // bps (reg 3), whitelist enforced at apply time
    uint16_t unlockDelayMs;     // pre-unlock delay (reg 80), 0-8000 ms
    LedPreset presets[8];       // color presets 1-8 (one physical ring)
};

/*  @brief Load configuration from the AT24 (with legacy import / recovery). */
void settingsInit();

/*  @brief Read access to the active configuration. */
const Settings& settings();

/*  @brief Mutable access for writers (Modbus persist path). Call
 *         settingsSave() afterwards to commit. */
Settings& settingsEdit();

/*  @brief Persist to the AT24 if the values changed.
 *  @return true if a write happened and succeeded */
bool settingsSave();

/*  @brief Restore factory defaults and persist them.
 *  @param keepId Preserve the current Modbus identifier */
void settingsFactoryReset(bool keepId);

/*  @brief false when the AT24 did not respond at boot (running on RAM
 *         defaults, nothing persists). */
bool settingsStorageOk();

/*  @brief Whitelist check for the baud rate register. */
bool settingsBaudAllowed(uint16_t baud);

#endif // SVC_SETTINGS_H
