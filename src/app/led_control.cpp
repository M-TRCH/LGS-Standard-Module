#include "app/led_control.h"
#include "config.h"
#include "app/latch_control.h"
#include "app/display_control.h"
#include "app/diag_control.h"
#include "drivers/led_ring.h"
#include "drivers/eeprom_at24.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"
#include "util/periodic_timer.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Preset engine: one physical ring, eight color presets (radio semantics)
// ---------------------------------------------------------------------------
//
// Coils 1001-1008 select which preset drives the ring. Exactly one preset is
// active at a time: activating a new one switches the ring color immediately
// and clears the outgoing preset's coils (radio behavior), so reading the
// coils always shows the single active preset. Firmware coil writes go
// through mbCoilWrite, which syncs the CHANGE shadows — radio-clears never
// re-fire handlers.

namespace {

uint8_t activePreset = 0;                       // 0 = off, 1-8 = active preset
uint32_t onSinceMs = 0;                         // millis() when the ring turned on (0 = off)
uint32_t onCount[MB_LED_PRESET_COUNT] = {};     // per-preset on transitions
uint32_t onTimeS[MB_LED_PRESET_COUNT] = {};     // per-preset accumulated on-time (whole seconds)
uint16_t onTimeMsFrac[MB_LED_PRESET_COUNT] = {}; // sub-second remainder (<1000 ms)

// --- Persistent statistics (AT24 @ STATS_AT24_ADDR) -----------------------
// Survives reboots/OTA; flushed hourly and before every commanded reset.
// A missing/corrupt blob just starts the counters (and boot count) at zero.

struct StatsBlob
{
    uint32_t magic;                             // 'LGSS'
    uint16_t bootCount;
    uint16_t reserved;
    uint32_t onCount[MB_LED_PRESET_COUNT];
    uint32_t onTimeS[MB_LED_PRESET_COUNT];
    uint16_t crc;                               // CRC16-CCITT over magic..onTimeS
};
constexpr uint32_t STATS_MAGIC = 0x4C475353;    // 'LGSS'

uint16_t bootCount = 0;
StatsBlob persistedStats = {};                  // change-detection cache

uint16_t statsCrc16(const uint8_t *p, size_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--)
    {
        crc ^= (uint16_t)(*p++) << 8;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void statsFill(StatsBlob &b)
{
    b.magic = STATS_MAGIC;
    b.bootCount = bootCount;
    b.reserved = 0;
    memcpy(b.onCount, onCount, sizeof(onCount));
    memcpy(b.onTimeS, onTimeS, sizeof(onTimeS));
    b.crc = statsCrc16((const uint8_t *)&b, offsetof(StatsBlob, crc));
}

void statsPersistIfChanged()
{
    StatsBlob b;
    statsFill(b);
    if (memcmp(&b, &persistedStats, sizeof(b)) == 0)
    {
        return; // unchanged: spare the EEPROM the write cycle
    }
    if (at24Write(STATS_AT24_ADDR, (const uint8_t *)&b, sizeof(b)))
    {
        persistedStats = b;
    }
}

void statsLoad()
{
    StatsBlob b;
    if (at24Read(STATS_AT24_ADDR, (uint8_t *)&b, sizeof(b)) &&
        b.magic == STATS_MAGIC &&
        b.crc == statsCrc16((const uint8_t *)&b, offsetof(StatsBlob, crc)))
    {
        bootCount = b.bootCount;
        memcpy(onCount, b.onCount, sizeof(onCount));
        memcpy(onTimeS, b.onTimeS, sizeof(onTimeS));
    }
    // Count this boot and persist right away (one small write per boot).
    bootCount++;
    diagSetBootCount(bootCount);
    statsPersistIfChanged();
}

// Scale a color component register (0-255) by the brightness register
// (0-100 percent), clamping out-of-range register values.
uint8_t scaledComponent(uint16_t component, uint16_t brightness)
{
    if (component > 255)
    {
        component = 255;
    }
    return (uint8_t)((component * brightness) / 100);
}

// Apply preset n's configured RGB (from its Modbus register block), scaled
// by its brightness.
void applyPresetColor(uint8_t n)
{
    uint16_t base = mbRegLedBase(n);
    uint16_t brightness = mbRegRead(base + 0);
    if (brightness > 100)
    {
        brightness = 100;
    }
    ledSetAllPixels(0, ledColor(
        scaledComponent(mbRegRead(base + 1), brightness),
        scaledComponent(mbRegRead(base + 2), brightness),
        scaledComponent(mbRegRead(base + 3), brightness)));
}

// Close the active preset's on-interval and clear its state-coil mirrors
// (enable + display-combo). Does not touch the pixels or the display state.
void closeActivePreset()
{
    if (activePreset == 0)
    {
        return;
    }
    if (onSinceMs != 0)
    {
        // Accumulate in whole seconds + a <1s remainder, so lifetime totals
        // never overflow the way a u32 millisecond sum would (~49.7 days).
        uint32_t deltaMs = (millis() - onSinceMs) + onTimeMsFrac[activePreset - 1];
        onTimeS[activePreset - 1] += deltaMs / 1000;
        onTimeMsFrac[activePreset - 1] = (uint16_t)(deltaMs % 1000);
        onSinceMs = 0;
    }
    mbCoilWrite(mbCoilLedEnable(activePreset), false);
    mbCoilWrite(mbCoilLedDisplay(activePreset), false);
    activePreset = 0;
}

// Radio activation: switch the ring to preset n (closing whichever preset
// was active). Callers mirror the enable coil themselves — the bus-write
// path already has it set, while the latch combos sync it only when the
// pulse completes (legacy 1021 nuance).
void activatePreset(uint8_t n)
{
    if (activePreset == n)
    {
        applyPresetColor(n); // refresh (e.g. re-command with updated colors)
        return;
    }
    closeActivePreset();
    applyPresetColor(n);
    activePreset = n;
    onCount[n - 1]++;
    onSinceMs = millis();
}

// Ring off (from the active preset's coil, max-on-time, or a combo-off).
void deactivate()
{
    ledSetAllPixels(0, ledColor(0, 0, 0));
    closeActivePreset();
}

// --- Modbus handlers ---

// Enable coils (1001-1008): change-triggered preset select / off
void onLedEnableChange(uint16_t addr, uint16_t value)
{
    uint8_t n = (uint8_t)(addr - 1000);
    if (value)
    {
        activatePreset(n);
    }
    else if (activePreset == n)
    {
        deactivate();
    }
    // write-0 to an inactive preset's coil: nothing to do
}

// LED-latch coils (1021-1028): preset n on + safety latch pulse in one command
void onLedLatchCommand(uint16_t addr, uint16_t value)
{
    (void)value;

    if (latchBusyWith(addr))
    {
        return; // request in flight: coil stays set until the pulse resolves
    }

    uint8_t n = (uint8_t)(addr - 1020);
    activatePreset(n);

    // Hand the unlock to the latch state machine; on completion it clears
    // this coil and syncs the preset's enable coil. When busy (another
    // request or cooldown) the LED still turns on — same as the original
    // reject path — but the coils resolve immediately so the command is
    // not retried.
    if (!latchRequestUnlock(LATCH_PULSE_MS, addr, mbCoilLedEnable(n)))
    {
        mbCoilWrite(addr, false);
        mbCoilWrite(mbCoilLedEnable(n), true);
    }
}

// Light+display coils (1011-1018): state coils — preset n + display in one
// write. Mirrors the preset's enable coil immediately (state semantics) and
// drives the display module.
void onLedDisplayChange(uint16_t addr, uint16_t value)
{
    uint8_t n = (uint8_t)(addr - 1010);
    if (value)
    {
        activatePreset(n);
        mbCoilWrite(mbCoilLedEnable(n), true);
        displayControlSetEnabled(true);
    }
    else if (activePreset == n)
    {
        deactivate();
        displayControlSetEnabled(false);
    }
    // write-0 to an inactive preset's combo: nothing to do
}

// Light+latch+display coils (1031-1038, R5.0-new): one command lights the
// preset, shows the display number, and fires a safety latch pulse. Display
// feedback happens at accept (the pulse resolves asynchronously); the enable
// coil syncs after the pulse like the plain latch combos.
void onLedLatchDisplayCommand(uint16_t addr, uint16_t value)
{
    (void)value;

    if (latchBusyWith(addr))
    {
        return; // request in flight: coil stays set until the pulse resolves
    }

    uint8_t n = (uint8_t)(addr - 1030);
    activatePreset(n);
    displayControlSetEnabled(true);
    // Mirror the preset+display STATE coil right away (the display part is
    // already active), so one write of 101N=0 later shuts both the ring and
    // the display — symmetric with turning a 1021 command off via 1001=0.
    mbCoilWrite(mbCoilLedDisplay(n), true);

    if (!latchRequestUnlock(LATCH_PULSE_MS, addr, mbCoilLedEnable(n)))
    {
        mbCoilWrite(addr, false);
        mbCoilWrite(mbCoilLedEnable(n), true);
    }
}

// --- Identify (coil 509) ---------------------------------------------------
// Blink the ring white so the unit can be spotted in a wall of lockers.
// Pure overlay: preset state, coils and statistics are untouched; when the
// window ends the ring goes back to whatever the active preset says.

bool identifyActive = false;
uint32_t identifyStartMs = 0;

void onIdentifyCommand(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbCoilWrite(MB_COIL_IDENTIFY, false);
    identifyActive = true;
    identifyStartMs = millis();
}

void identifyOverlayTick(uint32_t now)
{
    static bool phaseOn = false;
    static uint32_t lastToggleMs = 0;

    if (!identifyActive)
    {
        return;
    }
    if (now - identifyStartMs >= IDENTIFY_DURATION_MS)
    {
        identifyActive = false;
        // Hand the ring back to the preset engine.
        if (activePreset != 0)
        {
            applyPresetColor(activePreset);
        }
        else
        {
            ledSetAllPixels(0, ledColor(0, 0, 0));
        }
        return;
    }
    if (now - lastToggleMs >= IDENTIFY_BLINK_MS)
    {
        lastToggleMs = now;
        phaseOn = !phaseOn;
        uint8_t w = phaseOn ? IDENTIFY_WHITE_LEVEL : 0;
        ledSetAllPixels(0, ledColor(w, w, w));
    }
}

// Clear statistics (coil 510): zero every counter, on the wire and in the
// persistent blob.
void onClearStatsCommand(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbCoilWrite(MB_COIL_CLEAR_STATS, false);
    ledControlClearStats();
}

// All Off (coil 511): one command back to the resting state from ANY coil
// configuration — ring off, display blank, every preset mirror cleared.
// Also the simple recovery when a master has lost track of the coil state.
void onAllOffCommand(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbCoilWrite(MB_COIL_ALL_OFF, false);

    deactivate(); // ring off + active preset's mirrors cleared
    displayControlSetEnabled(false);
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        mbCoilWrite(mbCoilLedEnable(n), false);
        mbCoilWrite(mbCoilLedDisplay(n), false);
    }
}

// Global brightness (190): fan out to every preset's brightness register.
// Out-of-range writes clamp to 100 and the clamped value is reflected back
// (a silently-ignored write would leave the register claiming a value that
// was never applied). Persist-style semantics: no re-apply to already-lit
// pixels (takes effect at the next enable edge), matching the original.
void onGlobalBrightnessChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    if (value > 100)
    {
        value = 100;
        mbRegWrite(MB_REG_GLOBAL_BRIGHTNESS, value);
    }
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        mbRegWrite(mbRegLedBase(n) + 0, value);
    }
}

// Global max on-time (194): fan out to every preset's max-on-time register.
void onGlobalMaxOnTimeChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        mbRegWrite(mbRegLedBase(n) + 4, value);
    }
}

uint16_t clampToU16(uint32_t value)
{
    return (value > 65535) ? 65535 : (uint16_t)value;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ledControlInit()
{
    statsLoad(); // counters + boot count from the AT24 (zeros when blank)

    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        mbRegisterHandler(MB_WATCH_COIL_CHANGE, mbCoilLedEnable(n), onLedEnableChange);
        mbRegisterHandler(MB_WATCH_COIL_CHANGE, mbCoilLedDisplay(n), onLedDisplayChange);
        mbRegisterHandler(MB_WATCH_COIL_COMMAND, mbCoilLedLatch(n), onLedLatchCommand);
        mbRegisterHandler(MB_WATCH_COIL_COMMAND, mbCoilLedLatchDisplay(n), onLedLatchDisplayCommand);
    }
    mbRegisterHandler(MB_WATCH_REG_CHANGE, MB_REG_GLOBAL_BRIGHTNESS, onGlobalBrightnessChange);
    mbRegisterHandler(MB_WATCH_REG_CHANGE, MB_REG_GLOBAL_MAX_ON_TIME, onGlobalMaxOnTimeChange);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_IDENTIFY, onIdentifyCommand);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_CLEAR_STATS, onClearStatsCommand);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_ALL_OFF, onAllOffCommand);
}

void ledControlPersistStats()
{
    // Close the running interval into the accumulators first, so a flush
    // right before a reset captures the in-flight on-time too.
    if (activePreset != 0 && onSinceMs != 0)
    {
        uint32_t nowMs = millis();
        uint32_t deltaMs = (nowMs - onSinceMs) + onTimeMsFrac[activePreset - 1];
        onTimeS[activePreset - 1] += deltaMs / 1000;
        onTimeMsFrac[activePreset - 1] = (uint16_t)(deltaMs % 1000);
        onSinceMs = nowMs;
    }
    statsPersistIfChanged();
}

void ledControlClearStats()
{
    memset(onCount, 0, sizeof(onCount));
    memset(onTimeS, 0, sizeof(onTimeS));
    memset(onTimeMsFrac, 0, sizeof(onTimeMsFrac));
    if (onSinceMs != 0)
    {
        onSinceMs = millis(); // restart the in-flight interval from zero
    }
    statsPersistIfChanged();
}

bool ledControlChannelOn()
{
    return activePreset != 0;
}

uint16_t ledControlActiveEnableCoil()
{
    return (activePreset != 0) ? mbCoilLedEnable(activePreset) : 0;
}

uint8_t ledControlActivePreset()
{
    return activePreset;
}

void ledControlTick(uint32_t now)
{
    // Enforce the ACTIVE preset's max-on-time limit (0 = unlimited)
    if (activePreset != 0 && onSinceMs != 0)
    {
        uint16_t maxOnTimeS = mbRegRead(mbRegLedBase(activePreset) + 4);
        if (maxOnTimeS > 0 && now - onSinceMs > (uint32_t)maxOnTimeS * 1000)
        {
            deactivate(); // ring off + coil mirrors cleared (display state untouched)
        }
    }

    // Publish statistics (clamped to uint16 range, seconds truncated)
    uint32_t totalCount = 0;
    uint32_t totalTimeS = 0;
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        uint32_t timeS = onTimeS[n - 1];
        mbRegWrite(mbRegLedOnCounter(n), clampToU16(onCount[n - 1]));
        mbRegWrite(mbRegLedOnTime(n), clampToU16(timeS));
        totalCount += onCount[n - 1];
        totalTimeS += timeS;
    }
    mbRegWrite(MB_REG_TOTAL_LED_ON_CNT, clampToU16(totalCount));
    mbRegWrite(MB_REG_TOTAL_LED_ON_TIME, clampToU16(totalTimeS));

    // Hourly flush to the AT24 (writes only when something changed); a
    // flush also runs before every commanded reset via opsSystemReset.
    static PeriodicTimer statsPersistTimer{STATS_PERSIST_INTERVAL_MS};
    if (statsPersistTimer.due(now))
    {
        ledControlPersistStats();
    }

    // Identify overlay renders last so it wins the frame while active.
    identifyOverlayTick(now);
}
