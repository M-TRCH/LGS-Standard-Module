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

// R5.0 map: coils end at MB_COIL_LED_1_LATCH (1021), registers at 211 plus
// documented reserve. Addresses outside the model raise Modbus exceptions.
constexpr uint16_t COIL_NUM             = 1030;
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
    { MB_REG_LED_1_BRIGHTNESS,  offsetof(Settings, ledBrightness) },
    { MB_REG_LED_1_RED,         offsetof(Settings, ledR) },
    { MB_REG_LED_1_GREEN,       offsetof(Settings, ledG) },
    { MB_REG_LED_1_BLUE,        offsetof(Settings, ledB) },
    { MB_REG_LED_1_MAX_ON_TIME, offsetof(Settings, ledMaxOnTimeS) },
};

uint16_t& settingsFieldAt(Settings &s, uint8_t offset)
{
    return *(uint16_t *)((uint8_t *)&s + offset);
}

// --- Watch table: bus writes -> app handlers ---
constexpr uint8_t MB_MAX_WATCH_ROWS = 16;

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
}

void mbRegistersToSettings(bool save)
{
    Settings &s = settingsEdit();
    uint16_t previousId = s.identifier;

    for (const PersistRow &row : kPersistRows)
    {
        settingsFieldAt(s, row.offset) = RTUServer.holdingRegisterRead(row.addr);
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
