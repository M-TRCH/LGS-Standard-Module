#include "svc/settings.h"
#include "config.h"
#include "drivers/eeprom_at24.h"

// ---------------------------------------------------------------------------
// Storage format
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t SETTINGS_MAGIC   = 0x4C475335; // 'LGS5'
constexpr uint16_t SETTINGS_SCHEMA  = 2;          // v2 = 8 LED presets
constexpr uint16_t SETTINGS_AT24_ADDR = 0;        // blob lives at offset 0

// Schema v1 payload (single LED channel) — kept only for the one-time
// v1 -> v2 migration in settingsInit.
struct SettingsV1
{
    uint16_t identifier;
    uint16_t baudRate;
    uint16_t unlockDelayMs;
    uint16_t ledBrightness;
    uint16_t ledR;
    uint16_t ledG;
    uint16_t ledB;
    uint16_t ledMaxOnTimeS;
};

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
    .presets       = DEFAULT_LED_PRESETS,
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

uint16_t clampU16(uint16_t value, uint16_t maxValue)
{
    return (value > maxValue) ? maxValue : value;
}

// NOTE: the pre-AT24 legacy importer (MCU-flash EEPROM emulation) was removed
// for OTA: the emulation lived in the LAST flash page (0x0801F800), which is
// now part of the OTA staging area — its tombstone write would corrupt a
// staged image, and staged image bytes could in turn fool the importer's
// plausibility check. Every fielded R5.0 board already carries an AT24 blob.

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

        // One-time v1 -> v2 migration: the v1 payload+CRC live inside what a
        // v2-sized read parsed as payload bytes, so re-slice the raw buffer.
        // Payload is committed before the (v2) header, so a migration torn by
        // power loss leaves a v1 header over v2 bytes -> the v1 CRC fails ->
        // the torn path below still salvages the identifier.
        bool migratedV1 = false;
        if (!intact && blob.schemaVersion == 1 && blob.payloadSize == sizeof(SettingsV1))
        {
            const uint8_t *raw = (const uint8_t *)&blob;
            SettingsV1 v1;
            uint16_t crcV1;
            memcpy(&v1, raw + BLOB_HEADER_SIZE, sizeof(v1));
            memcpy(&crcV1, raw + BLOB_HEADER_SIZE + sizeof(v1), sizeof(crcV1));
            if (crcV1 == crc16((const uint8_t *)&v1, sizeof(v1)))
            {
                active = kDefaults;
                if (v1.identifier >= 1 && v1.identifier <= 247)
                {
                    active.identifier = v1.identifier;
                }
                active.baudRate      = settingsBaudAllowed(v1.baudRate) ? v1.baudRate : DEFAULT_BAUD_RATE;
                active.unlockDelayMs = clampU16(v1.unlockDelayMs, UNLOCK_DELAY_MAX_MS);
                active.presets[0].brightness = clampU16(v1.ledBrightness, 100);
                active.presets[0].r          = clampU16(v1.ledR, 255);
                active.presets[0].g          = clampU16(v1.ledG, 255);
                active.presets[0].b          = clampU16(v1.ledB, 255);
                active.presets[0].maxOnTimeS = v1.ledMaxOnTimeS;
                // presets 2-8 keep the default palette
                if (writePayload(active))
                {
                    writeHeader(); // header last: schema/size flip to v2 only above a full payload
                }
                migratedV1 = true;
            }
        }

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
        else if (migratedV1)
        {
            // active already holds the migrated configuration
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
        // No valid magic: first boot on this AT24 — format with defaults.
        // Payload goes first, the header (magic) last, so a torn format
        // leaves a blank-looking chip rather than a valid-looking blob.
        if (writePayload(active))
        {
            writeHeader();
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
