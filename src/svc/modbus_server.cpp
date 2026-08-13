#include "svc/modbus_server.h"
#include <ModbusRTUServer.h>
#include "svc/modbus_map.h"
#include "svc/settings.h"
#include "svc/commission.h"
#include "drivers/rs485_port.h"
#include "version.h"

// ---------------------------------------------------------------------------
// Modbus data model
// ---------------------------------------------------------------------------

namespace {

// R5.0 map: coils end at the latch+display combos (1031-1038), registers at
// the Statistics-v2 block (451). Addresses outside the model raise Modbus
// exceptions — which is exactly how a v3.2.0 master learns this firmware
// has no 400+ block, so the ceiling is part of the wire contract.
constexpr uint16_t COIL_NUM             = 1040;
constexpr uint16_t DISCRETE_INPUT_NUM   = 1;
constexpr uint16_t HOLDING_REGISTER_NUM = MB_REG_S2_LAST + 1;   // = 452
constexpr uint16_t INPUT_REGISTER_NUM   = 1;

ModbusRTUServerClass RTUServer;

// Frame-gap state for modbusServerTick(): how long the line must be quiet
// before a frame counts as complete, what the ring held at the last look,
// when it last changed, and how many gaps a leftover has survived.
uint16_t _frameGapMs = 4;
int      _rxSeen = 0;
uint32_t _rxSince = 0;
uint8_t  _rxStale = 0;

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

    // 3.5 character times, the silence that ends an RTU frame: 8N1 is 10
    // bits per character, so 35 bit-times, rounded up and never below 2 ms
    // (the tick samples in whole milliseconds).
    _frameGapMs = (uint16_t)((35000UL + baud - 1) / baud) + 1;
    if (_frameGapMs < 2) _frameGapMs = 2;
    _rxSeen = 0;
    _rxSince = 0;
    _rxStale = 0;
}

void modbusServerTick()
{
    // A Modbus RTU frame is delimited by SILENCE, not by length — so wait
    // for that silence before handing the buffer to the library.
    //
    // This is not pedantry: ArduinoModbus receives with a BLOCKING
    // Stream::readBytes, so a frame that arrives incomplete leaves the
    // module sitting inside poll() until the 4 s watchdog reboots it. The
    // RS485 switch hub cuts a frame in half every time it changes channel,
    // which on the bench cost every module of the crossed channel ~10
    // reboots per 15 cabinet passes — invisible to the master (the module
    // answers again within a second) but every lit slot went dark, which
    // is a pick vanishing under the pharmacist's hand.
    //
    // Gating on the frame gap means that when poll() finally runs, every
    // byte of the frame is already in the ring (256 B, sized for the
    // largest ADU) and no read inside it can block on the wire.
    const int have = rs485.available();
    if (have <= 0)
    {
        _rxSeen = 0;
        _rxStale = 0;
        return;
    }

    const uint32_t now = millis();
    if (have != _rxSeen)
    {
        _rxSeen = have;             // still arriving: restart the gap
        _rxSince = now;
        return;
    }
    if (now - _rxSince < _frameGapMs)
    {
        return;                     // quiet, but not for long enough yet
    }

    const bool served = (RTUServer.poll() != 0);
    if (served)
    {
        watchScan();
        _rxStale = 0;
    }
    else if (++_rxStale >= 2)
    {
        // Bytes that survived a poll across two gaps are the wreckage of a
        // cut frame (or noise). Drop them: leaving them in front of the
        // next real request would corrupt that one too. Two gaps, not one,
        // so back-to-back frames — the OTA stream — are never thrown away
        // half-consumed.
        while (rs485.available())
        {
            rs485.read();
        }
        _rxStale = 0;
    }
    _rxSeen = rs485.available();
    _rxSince = millis();
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
    // Identity registers come from compile-time constants and the
    // silicon-burned UID so they always reflect the running device
    // (never stale EEPROM copies).
    //
    // Device type is the one exception, and deliberately so: one image now
    // serves cabinets the factory builds differently (LED mask, no ring/OLED
    // /big button vs the full build), and only commissioning knows which
    // board it is holding. A board never commissioned with a type reports
    // the compile-time default, so nothing already deployed changes.
    RTUServer.holdingRegisterWrite(MB_REG_DEVICE_TYPE, deviceTypeEffective());
    RTUServer.holdingRegisterWrite(MB_REG_FW_VERSION, FW_VERSION);
    RTUServer.holdingRegisterWrite(MB_REG_HW_VERSION, HW_VERSION);

    // Hi word first per 32-bit UID word: hex-concatenating regs 12..17
    // reproduces exactly the serial the commissioning bench reads over SWD
    // and records in commission_log.csv — one number, both transports.
    const uint32_t uidWords[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
    for (uint8_t i = 0; i < 3; i++)
    {
        RTUServer.holdingRegisterWrite(MB_REG_UID_BASE + 2 * i,
                                       (uint16_t)(uidWords[i] >> 16));
        RTUServer.holdingRegisterWrite(MB_REG_UID_BASE + 2 * i + 1,
                                       (uint16_t)uidWords[i]);
    }

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
    uint16_t previousBaud = s.baudRate;

    for (const PersistRow &row : kPersistRows)
    {
        settingsFieldAt(s, row.offset) = RTUServer.holdingRegisterRead(row.addr);
    }
    // Preset fields clamp to their wire ranges (brightness 0-100, RGB 0-255)
    // and the clamped value is reflected back, so the registers, the EEPROM
    // and the color actually applied never disagree.
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        uint16_t *fields = presetFields(s, (uint8_t)(n - 1));
        for (uint8_t k = 0; k < LED_PRESET_FIELDS; k++)
        {
            uint16_t v = RTUServer.holdingRegisterRead(mbRegLedBase(n) + k);
            uint16_t maxV = (k == 0) ? 100 : (k <= 3) ? 255 : 65535;
            if (v > maxV)
            {
                v = maxV;
                RTUServer.holdingRegisterWrite(mbRegLedBase(n) + k, v);
            }
            fields[k] = v;
        }
    }

    // Reject an out-of-range slave ID before it is persisted: a value >255
    // would otherwise alias mod-256 at the next boot and answer at another
    // device's address; 246 is reserved for SET_ID discovery. Keep the
    // previous ID and reflect it back.
    if (s.identifier < 1 || s.identifier > 247 || s.identifier == 246)
    {
        s.identifier = previousId;
        RTUServer.holdingRegisterWrite(MB_REG_IDENTIFIER, previousId);
    }

    // Reject a baud outside the whitelist the same way — persisting garbage
    // would silently fall back to 9600 at the next boot while the register
    // kept claiming the bogus value.
    if (!settingsBaudAllowed(s.baudRate))
    {
        s.baudRate = previousBaud;
        RTUServer.holdingRegisterWrite(MB_REG_BAUD_RATE, previousBaud);
    }

    if (save)
    {
        settingsSave();
    }
}
