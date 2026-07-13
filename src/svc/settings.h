#ifndef SVC_SETTINGS_H
#define SVC_SETTINGS_H

#include <Arduino.h>

/*  @file svc/settings.h
 *  @brief Persistent configuration, stored on the external AT24C32D EEPROM.
 *
 *  The blob is versioned (magic + schema version + CRC16). Loading follows
 *  a strict order:
 *    1. valid blob on the AT24            -> use it
 *    2. no/foreign magic                  -> one-time import from the legacy
 *       MCU-flash EEPROM layout (keeps the fielded Modbus ID), then save
 *    3. valid magic but bad CRC (torn)    -> defaults, salvaging a plausible
 *       identifier; never falls through to the legacy importer
 *    4. AT24 absent                       -> defaults in RAM, nothing persists
 *       (settingsStorageOk() returns false)
 */

// Values persisted for the R/W(F) Modbus registers.
struct Settings
{
    uint16_t identifier;        // Modbus slave ID (reg 4)
    uint16_t baudRate;          // bps (reg 3), whitelist enforced at apply time
    uint16_t unlockDelayMs;     // pre-unlock delay (reg 80), 0-8000 ms
    uint16_t ledBrightness;     // LED_1 brightness 0-100 (reg 110)
    uint16_t ledR;              // LED_1 red 0-255 (reg 111)
    uint16_t ledG;              // LED_1 green 0-255 (reg 112)
    uint16_t ledB;              // LED_1 blue 0-255 (reg 113)
    uint16_t ledMaxOnTimeS;     // LED_1 max on-time in seconds (reg 114), 0 = unlimited
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
