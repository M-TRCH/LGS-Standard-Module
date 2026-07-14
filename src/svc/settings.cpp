#include "svc/settings.h"
#include <EEPROM.h>
#include "config.h"
#include "drivers/eeprom_at24.h"

// ---------------------------------------------------------------------------
// Storage format
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t SETTINGS_MAGIC   = 0x4C475335; // 'LGS5'
constexpr uint16_t SETTINGS_SCHEMA  = 1;
constexpr uint16_t SETTINGS_AT24_ADDR = 0;        // blob lives at offset 0

struct SettingsBlob
{
    uint32_t magic;
    uint16_t schemaVersion;
    uint16_t payloadSize;
    Settings payload;
    uint16_t crc;           // CRC16-CCITT over payload bytes
};

const Settings kDefaults =
{
    .identifier    = DEFAULT_IDENTIFIER,
    .baudRate      = DEFAULT_BAUD_RATE,
    .unlockDelayMs = DEFAULT_UNLOCK_DELAY_TIME,
    .ledBrightness = DEFAULT_LED_BRIGHTNESS,
    .ledR          = DEFAULT_LED_R,
    .ledG          = DEFAULT_LED_G,
    .ledB          = DEFAULT_LED_B,
    .ledMaxOnTimeS = DEFAULT_LED_MAX_ON_TIME,
};

Settings active;        // the live configuration
Settings persisted;     // copy of what the AT24 holds, for change detection
bool storageOk = false;

uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

constexpr uint16_t BLOB_HEADER_SIZE = offsetof(SettingsBlob, payload);

// Write only the payload + CRC bytes. The header (with the magic) is never
// touched by routine saves, so a save torn by power loss can only produce a
// bad-CRC blob — the recovery path that salvages the identifier — and never
// a missing magic (which would look like a blank chip).
bool writePayload(const Settings &s)
{
    SettingsBlob blob;
    blob.payload = s;
    blob.crc = crc16((const uint8_t *)&blob.payload, sizeof(Settings));
    return at24Write(SETTINGS_AT24_ADDR + BLOB_HEADER_SIZE,
                     (const uint8_t *)&blob + BLOB_HEADER_SIZE,
                     sizeof(SettingsBlob) - BLOB_HEADER_SIZE);
}

// One-time format: commit the header AFTER the payload so the magic only
// ever appears above a valid payload.
bool writeHeader()
{
    SettingsBlob blob;
    blob.magic = SETTINGS_MAGIC;
    blob.schemaVersion = SETTINGS_SCHEMA;
    blob.payloadSize = sizeof(Settings);
    return at24Write(SETTINGS_AT24_ADDR, (const uint8_t *)&blob, BLOB_HEADER_SIZE);
}

// ---------------------------------------------------------------------------
// Legacy import: EepromConfig_t layout in the MCU flash-emulated EEPROM,
// as written by firmware builds before the AT24 migration. Kept isolated
// so it can be deleted once no R5.0 board with old data remains.
// ---------------------------------------------------------------------------

struct LegacyEepromConfig
{
    bool isFirstBootExceptID;
    bool isFirstBoot;
    uint16_t deviceType;
    uint16_t fwVersion;
    uint16_t hwVersion;
    uint16_t baudRate;
    uint16_t identifier;
    uint16_t led_brightness[8];
    uint16_t led_r[8];
    uint16_t led_g[8];
    uint16_t led_b[8];
    uint16_t maxOnTime[8];
    uint16_t ledNumPerStrip;
    uint16_t unlockDelayTime;
};

uint16_t clampU16(uint16_t value, uint16_t maxValue)
{
    return (value > maxValue) ? maxValue : value;
}

// Try to carry a fielded configuration over from the MCU flash. Returns
// false when the flash content is not a plausible legacy config (e.g. a
// factory-fresh chip full of 0xFF).
bool importLegacy(Settings &out)
{
    LegacyEepromConfig legacy;
    EEPROM.get(0, legacy);

    if (legacy.identifier < 1 || legacy.identifier > 247)
    {
        return false;
    }
    if (legacy.led_brightness[0] > 100)
    {
        return false;
    }

    out = kDefaults;
    out.identifier    = legacy.identifier;
    out.baudRate      = settingsBaudAllowed(legacy.baudRate) ? legacy.baudRate : DEFAULT_BAUD_RATE;
    out.unlockDelayMs = clampU16(legacy.unlockDelayTime, UNLOCK_DELAY_MAX_MS);
    out.ledBrightness = legacy.led_brightness[0];
    out.ledR          = clampU16(legacy.led_r[0], 255);
    out.ledG          = clampU16(legacy.led_g[0], 255);
    out.ledB          = clampU16(legacy.led_b[0], 255);
    out.ledMaxOnTimeS = legacy.maxOnTime[0];
    return true;
}

// Invalidate the MCU-flash copy after a successful migration, so a later
// AT24 fault can never resurrect stale pre-migration config through the
// importer (the identifier 0 fails importLegacy's plausibility check).
void tombstoneLegacy()
{
    uint16_t invalidId = 0;
    EEPROM.put(offsetof(LegacyEepromConfig, identifier), invalidId);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void settingsInit()
{
    active = kDefaults;

    storageOk = at24Probe();
    if (!storageOk)
    {
        // No EEPROM on the bus: run on defaults, nothing persists.
        persisted = active;
        return;
    }

    // Retry the boot read: a transient I2C fault must never be mistaken for
    // a blank chip — that path formats the EEPROM and would destroy a valid
    // stored configuration.
    SettingsBlob blob;
    bool readOk = false;
    for (uint8_t attempt = 0; attempt < 3 && !readOk; attempt++)
    {
        readOk = at24Read(SETTINGS_AT24_ADDR, (uint8_t *)&blob, sizeof(blob));
    }
    if (!readOk)
    {
        // Chip acks but reads keep failing: run on RAM defaults WITHOUT
        // writing anything, so the stored blob survives to the next boot.
        // storageOk=false also signals the fault via the fast RUN blink.
        storageOk = false;
        persisted = active;
        return;
    }

    if (blob.magic == SETTINGS_MAGIC)
    {
        bool intact = (blob.schemaVersion == SETTINGS_SCHEMA)
                   && (blob.payloadSize == sizeof(Settings))
                   && (blob.crc == crc16((const uint8_t *)&blob.payload, sizeof(Settings)));
        if (intact)
        {
            active = blob.payload;
            // Never trust an out-of-range slave ID into the RTU stack (it
            // would alias mod-256 and answer at another device's address).
            if (active.identifier < 1 || active.identifier > 247)
            {
                active.identifier = DEFAULT_IDENTIFIER;
            }
        }
        else
        {
            // Torn payload write: defaults, but salvage a plausible
            // identifier so the device does not silently drop off the bus.
            // NEVER falls through to the legacy importer (the bytes are
            // new-format, just damaged).
            if (blob.payload.identifier >= 1 && blob.payload.identifier <= 246)
            {
                active.identifier = blob.payload.identifier;
            }
            writePayload(active);
        }
    }
    else
    {
        // No valid magic: first boot on this AT24. Import a fielded config
        // from the MCU flash if one is there, otherwise start from defaults.
        // Payload goes first, the header (magic) last; the legacy copy is
        // tombstoned only after the new blob is fully committed.
        Settings imported;
        bool didImport = importLegacy(imported);
        if (didImport)
        {
            active = imported;
        }
        if (writePayload(active) && writeHeader() && didImport)
        {
            tombstoneLegacy();
        }
    }

    persisted = active;
}

const Settings& settings()
{
    return active;
}

Settings& settingsEdit()
{
    return active;
}

bool settingsSave()
{
    if (!storageOk)
    {
        return false;
    }
    if (memcmp(&active, &persisted, sizeof(Settings)) == 0)
    {
        return false; // unchanged, avoid the write cycle
    }
    if (!writePayload(active))
    {
        return false;
    }
    persisted = active;
    return true;
}

void settingsFactoryReset(bool keepId)
{
    uint16_t savedId = active.identifier;
    active = kDefaults;
    if (keepId)
    {
        active.identifier = savedId;
    }
    settingsSave();
}

bool settingsStorageOk()
{
    return storageOk;
}

bool settingsBaudAllowed(uint16_t baud)
{
    // Reg 3 is a 16-bit register, so rates above 57600 cannot be represented.
    return (baud == 9600) || (baud == 19200) || (baud == 38400) || (baud == 57600);
}
