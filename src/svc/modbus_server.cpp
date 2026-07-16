#include "svc/modbus_server.h"
#include <ModbusRTUServer.h>
#include "svc/modbus_map.h"
#include "svc/settings.h"
#include "drivers/rs485_port.h"
#include "version.h"

// ---------------------------------------------------------------------------
// Modbus data model
// ---------------------------------------------------------------------------

namespace {

// R5.0 map: coils end at the latch+display combos (1031-1038), registers at
// the per-preset stats (281) plus documented reserve. Addresses outside the
// model raise Modbus exceptions.
constexpr uint16_t COIL_NUM             = 1040;
constexpr uint16_t DISCRETE_INPUT_NUM   = 1;
constexpr uint16_t HOLDING_REGISTER_NUM = 400;
constexpr uint16_t INPUT_REGISTER_NUM   = 1;

ModbusRTUServerClass RTUServer;

// --- Persist table: R/W(F) register <-> Settings field ---
struct PersistRow
{
    uint16_t addr;
    uint8_t  offset;    // offsetof(Settings, field); every field is uint16_t
};

const PersistRow kPersistRows[] =
{
    { MB_REG_BAUD_RATE,         offsetof(Settings, baudRate) },
    { MB_REG_IDENTIFIER,        offsetof(Settings, identifier) },
    { MB_REG_UNLOCK_DELAY,      offsetof(Settings, unlockDelayMs) },
};

uint16_t& settingsFieldAt(Settings &s, uint8_t offset)
{
    return *(uint16_t *)((uint8_t *)&s + offset);
}

// The 8 LED presets are handled by computed loops instead of 40 table rows:
// preset n's five fields {brightness,r,g,b,maxOnTimeS} map in order onto
// regs mbRegLedBase(n)+0..+4. The layout equivalence is asserted here.
static_assert(sizeof(LedPreset) == 5 * sizeof(uint16_t), "LedPreset is the 5-reg wire block");
constexpr uint8_t LED_PRESET_FIELDS = 5;

uint16_t* presetFields(Settings &s, uint8_t presetIdx)   // presetIdx = 0..7
{
    return (uint16_t *)&s.presets[presetIdx];
}

// --- Watch table: bus writes -> app handlers ---
// Sized for the full preset surface (41 rows: ops 3 + latch 2 + enables 8 +
// latch combos 8 + display combos 8 + triple combos 8 + reg 60/coil 1010 2 +
// globals 2) plus the OTA family (coils 505-508 + commit reg = 5) and
// headroom. mbRegisterHandler drops registrations SILENTLY when this is
// full — bump it BEFORE adding handler families.
constexpr uint8_t MB_MAX_WATCH_ROWS = 56;

struct WatchRow
{
    uint16_t addr;
    MbWatchKind kind;
    uint16_t shadow;
    MbWatchHandler handler;
};

WatchRow watchRows[MB_MAX_WATCH_ROWS];
uint8_t watchCount = 0;

uint16_t watchReadValue(const WatchRow &row)
{
    if (row.kind == MB_WATCH_REG_CHANGE)
    {
        return RTUServer.holdingRegisterRead(row.addr);
    }
    return (uint16_t)RTUServer.coilRead(row.addr);
}

void watchScan()
{
    for (uint8_t i = 0; i < watchCount; i++)
    {
        WatchRow &row = watchRows[i];
        uint16_t value = watchReadValue(row);

        if (row.kind == MB_WATCH_COIL_COMMAND)
        {
            if (value)
            {
                row.handler(row.addr, value);
            }
        }
        else if (value != row.shadow)
        {
            row.shadow = value;
            row.handler(row.addr, value);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void modbusServerInit(uint16_t slaveId, uint32_t baud)
{
    RTUServer.begin(rs485, slaveId, baud, SERIAL_8N1);

    RTUServer.configureCoils(0x00, COIL_NUM);
    RTUServer.configureDiscreteInputs(0x00, DISCRETE_INPUT_NUM);
    RTUServer.configureHoldingRegisters(0x00, HOLDING_REGISTER_NUM);
    RTUServer.configureInputRegisters(0x00, INPUT_REGISTER_NUM);
}

void modbusServerTick()
{
    if (RTUServer.poll())
    {
        watchScan();
    }
}

void mbRegisterHandler(MbWatchKind kind, uint16_t addr, MbWatchHandler handler)
{
    if (watchCount >= MB_MAX_WATCH_ROWS)
    {
        return; // table full: a registration was silently dropped -> bump MB_MAX_WATCH_ROWS
    }
    watchRows[watchCount] = { addr, kind, 0, handler };
    watchCount++;
}

void mbWatchSeedShadows()
{
    for (uint8_t i = 0; i < watchCount; i++)
    {
        watchRows[i].shadow = watchReadValue(watchRows[i]);
    }
}

uint16_t mbRegRead(uint16_t addr)
{
    return RTUServer.holdingRegisterRead(addr);
}

// Firmware-initiated writes also update any matching CHANGE shadow, so only
// bus-side writes fire handlers (matching the original last_* bookkeeping).
static void watchSyncShadow(MbWatchKind kind, uint16_t addr, uint16_t value)
{
    for (uint8_t i = 0; i < watchCount; i++)
    {
        if (watchRows[i].kind == kind && watchRows[i].addr == addr)
        {
            watchRows[i].shadow = value;
        }
    }
}

void mbRegWrite(uint16_t addr, uint16_t value)
{
    RTUServer.holdingRegisterWrite(addr, value);
    watchSyncShadow(MB_WATCH_REG_CHANGE, addr, value);
}

bool mbCoilRead(uint16_t addr)
{
    return (RTUServer.coilRead(addr) == 1);
}

void mbCoilWrite(uint16_t addr, bool value)
{
    RTUServer.coilWrite(addr, value ? 1 : 0);
    watchSyncShadow(MB_WATCH_COIL_CHANGE, addr, value ? 1 : 0);
}

void mbSettingsToRegisters()
{
    // Identity registers come from compile-time constants so they always
    // reflect the running build (never stale EEPROM copies).
    RTUServer.holdingRegisterWrite(MB_REG_DEVICE_TYPE, DEVICE_TYPE);
    RTUServer.holdingRegisterWrite(MB_REG_FW_VERSION, FW_VERSION);
    RTUServer.holdingRegisterWrite(MB_REG_HW_VERSION, HW_VERSION);

    const Settings &s = settings();
    for (const PersistRow &row : kPersistRows)
    {
        RTUServer.holdingRegisterWrite(row.addr, settingsFieldAt(const_cast<Settings &>(s), row.offset));
    }
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        const uint16_t *fields = presetFields(const_cast<Settings &>(s), (uint8_t)(n - 1));
        for (uint8_t k = 0; k < LED_PRESET_FIELDS; k++)
        {
            RTUServer.holdingRegisterWrite(mbRegLedBase(n) + k, fields[k]);
        }
    }
}

void mbRegistersToSettings(bool save)
{
    Settings &s = settingsEdit();
    uint16_t previousId = s.identifier;

    for (const PersistRow &row : kPersistRows)
    {
        settingsFieldAt(s, row.offset) = RTUServer.holdingRegisterRead(row.addr);
    }
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        uint16_t *fields = presetFields(s, (uint8_t)(n - 1));
        for (uint8_t k = 0; k < LED_PRESET_FIELDS; k++)
        {
            fields[k] = RTUServer.holdingRegisterRead(mbRegLedBase(n) + k);
        }
    }

    // Reject an out-of-range slave ID before it is persisted: a value >255
    // would otherwise alias mod-256 at the next boot and answer at another
    // device's address. Keep the previous ID and reflect it back.
    if (s.identifier < 1 || s.identifier > 247)
    {
        s.identifier = previousId;
        RTUServer.holdingRegisterWrite(MB_REG_IDENTIFIER, previousId);
    }

    if (save)
    {
        settingsSave();
    }
}
