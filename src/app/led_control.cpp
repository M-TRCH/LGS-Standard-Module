#include "app/led_control.h"
#include "config.h"
#include "app/latch_control.h"
#include "app/display_control.h"
#include "drivers/led_ring.h"
#include "drivers/led_mask.h"
#include "svc/commission.h"
#include "version.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"
#include "svc/stats.h"
#include "util/periodic_timer.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Two engines behind one coil family, chosen once at init by the display
// ---------------------------------------------------------------------------
//
// RING boards (type 20 and every other ring type) — radio semantics.
// Coils 1001-1008 select which preset drives the single ring. Exactly one
// preset is active at a time: activating a new one switches the ring color
// immediately and clears the outgoing preset's coils, so reading the coils
// always shows the single active preset.
//
// MASK boards (type 10 STANDARD, v3.5.0) — independent windows. The eight
// mask windows are eight separate lights: window n = person n, because the
// product reason for eight colors is up to eight people's medications being
// picked from ONE slot at the same time — the semantics the R4.x fleet has
// run in the field since 2025 (eight strips, eight pins, no radio). So on a
// mask board each coil switches its own window and any subset may be lit.
// Radio code never executes on mask boards and window code never executes
// on ring boards, which is what keeps this addition invisible to the
// type-20 cabinets already in service.
//
// Firmware coil writes go through mbCoilWrite, which syncs the CHANGE
// shadows — engine-made clears never re-fire handlers.

namespace {

uint8_t activePreset = 0;                       // 0 = off, 1-8 = active preset
uint32_t onSinceMs = 0;                         // millis() when the ring turned on (0 = off)
uint16_t onTimeMsFrac[MB_LED_PRESET_COUNT] = {}; // sub-second remainder (<1000 ms)

// Storage and publication of the counters moved to svc/stats (v3.3.0): this
// module keeps only the in-flight interval state above and reports whole
// seconds / on-transitions into the stats service as they happen.

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
// A board has one display or the other, never both: STANDARD cabinets are
// built with the 8-LED index mask and no ring (nor OLED, nor big button),
// every other type with the ring. Set once at init from the commissioned
// device type, so a bench board carrying both does not mirror onto the ring
// a production board will not have.
bool useMask = false;

// Every display update goes through here, so there is one copy of the rule
// rather than two device-specific paths through the preset engine. @p index
// selects which mask pixel (1-8 = that preset's index, 0 = the whole mask,
// used when there is no preset to point at — identify, or going dark).
void paintRing(uint32_t color, uint8_t index)
{
    if (!useMask)
    {
        ledSetAllPixels(0, color);
        return;
    }
    if (index >= 1 && index <= HW_LED_MASK_PIXEL_COUNT)
    {
        maskShowIndex(index, color);
    }
    else
    {
        maskSetAll(color);
    }
}

uint32_t presetColorOf(uint8_t n)
{
    uint16_t base = mbRegLedBase(n);
    uint16_t brightness = mbRegRead(base + 0);
    if (brightness > 100)
    {
        brightness = 100;
    }
    return ledColor(
        scaledComponent(mbRegRead(base + 1), brightness),
        scaledComponent(mbRegRead(base + 2), brightness),
        scaledComponent(mbRegRead(base + 3), brightness));
}

void applyPresetColor(uint8_t n)
{
    paintRing(presetColorOf(n), n);
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
        statsAddOnTime(activePreset, deltaMs / 1000);
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
    statsNoteLedOn(n);
    onSinceMs = millis();
}

// Ring off (from the active preset's coil, max-on-time, or a combo-off).
void deactivate()
{
    paintRing(ledColor(0, 0, 0), 0);
    closeActivePreset();
}

// --- Window engine (mask boards only) ---------------------------------------

uint8_t winLitMask = 0;                          // bit n-1 = window n lit
uint8_t winLastCmd = 0;                          // last window commanded on
uint32_t winOnSinceMs[MB_LED_PRESET_COUNT] = {}; // 0 = that window is off

// Repaint the whole lit set from the preset registers. Always the full set,
// one driver call: partial updates would keep stale colors on screen after
// a color-register write to an already-lit window.
void windowRepaint()
{
    uint32_t colors[MB_LED_PRESET_COUNT];
    for (uint8_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        colors[n - 1] = (winLitMask & (1u << (n - 1))) ? presetColorOf(n) : 0;
    }
    maskShowSet(winLitMask, colors);
}

// Fold window n's running on-interval into the statistics — the ring
// engine's whole-seconds + sub-second-remainder scheme, sharing
// onTimeMsFrac because only one engine ever runs on a given board.
void windowFoldOnTime(uint8_t n, bool restart)
{
    if (winOnSinceMs[n - 1] == 0)
    {
        return;
    }
    uint32_t nowMs = millis();
    uint32_t deltaMs = (nowMs - winOnSinceMs[n - 1]) + onTimeMsFrac[n - 1];
    statsAddOnTime(n, deltaMs / 1000);
    onTimeMsFrac[n - 1] = (uint16_t)(deltaMs % 1000);
    winOnSinceMs[n - 1] = restart ? nowMs : 0;
}

void windowOn(uint8_t n)
{
    winLastCmd = n;
    const uint8_t bit = (uint8_t)(1u << (n - 1));
    if (!(winLitMask & bit))
    {
        winLitMask |= bit;
        statsNoteLedOn(n);
        winOnSinceMs[n - 1] = millis();
    }
    windowRepaint(); // a re-command refreshes the color, like the ring path
}

// Window n off: fold its on-time and clear ITS coil mirrors only — the
// siblings keep shining, which is the point of this engine.
void windowOff(uint8_t n)
{
    const uint8_t bit = (uint8_t)(1u << (n - 1));
    if (!(winLitMask & bit))
    {
        return;
    }
    windowFoldOnTime(n, false);
    winLitMask &= (uint8_t)~bit;
    mbCoilWrite(mbCoilLedEnable(n), false);
    mbCoilWrite(mbCoilLedDisplay(n), false);
    windowRepaint();
}

// --- Modbus handlers ---

// Enable coils (1001-1008): change-triggered preset select / off
void onLedEnableChange(uint16_t addr, uint16_t value)
{
    uint8_t n = (uint8_t)(addr - 1000);
    if (useMask) // independent windows: this coil touches window n alone
    {
        if (value)
        {
            windowOn(n);
        }
        else
        {
            windowOff(n);
        }
        return;
    }
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
    if (useMask)
    {
        windowOn(n); // light this window; sibling windows stay lit
    }
    else
    {
        activatePreset(n);
    }

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
    if (useMask) // no OLED on a mask board; keep the coil semantics anyway
    {
        if (value)
        {
            windowOn(n);
            mbCoilWrite(mbCoilLedEnable(n), true);
            displayControlSetEnabled(true);
        }
        else if (winLitMask & (1u << (n - 1)))
        {
            windowOff(n);
            displayControlSetEnabled(false);
        }
        return;
    }
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
    if (useMask)
    {
        windowOn(n);
    }
    else
    {
        activatePreset(n);
    }
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
uint32_t identifyWindowMs = IDENTIFY_DURATION_MS;   // per-activation length
// Color of the blink's ON phase, FROZEN at activation. Frozen on purpose:
// the confirm ack races the master — a fast master clears the preset within
// its next poll, mid-blink — and an ack that consults live state would
// finish in the wrong color exactly when the system works best.
uint32_t identifyOnColor = 0;
uint8_t identifyMaskIndex = 0;      // frozen with the color, same reason
bool identifyPhaseOn = false;
uint32_t identifyLastToggleMs = 0;

void identifyStart(uint32_t windowMs, uint32_t onColor, uint8_t maskIndex)
{
    identifyActive = true;
    identifyStartMs = millis();
    identifyWindowMs = windowMs;
    identifyOnColor = onColor;
    identifyMaskIndex = maskIndex;
    // Reset the phase so the first visible half-period is the ON color —
    // an ack that opens on a dark phase reads as a glitch, not an answer.
    identifyPhaseOn = false;
    identifyLastToggleMs = 0;
}

void onIdentifyCommand(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbCoilWrite(MB_COIL_IDENTIFY, false);
    const uint8_t w = IDENTIFY_WHITE_LEVEL;
    // Index 0: "find this module" is about the whole unit, so the mask
    // blinks entire rather than pointing at one slot.
    identifyStart(IDENTIFY_DURATION_MS, ledColor(w, w, w), 0);
}

void identifyOverlayTick(uint32_t now)
{
    if (!identifyActive)
    {
        return;
    }
    if (now - identifyStartMs >= identifyWindowMs)
    {
        identifyActive = false;
        // Hand the display back to whichever engine owns this board.
        if (useMask)
        {
            windowRepaint(); // repaints the lit set; all-dark when none
        }
        else if (activePreset != 0)
        {
            applyPresetColor(activePreset);
        }
        else
        {
            paintRing(ledColor(0, 0, 0), 0);
        }
        return;
    }
    if (identifyLastToggleMs == 0
        || now - identifyLastToggleMs >= IDENTIFY_BLINK_MS)
    {
        identifyLastToggleMs = now;
        identifyPhaseOn = !identifyPhaseOn;
        paintRing(identifyPhaseOn ? identifyOnColor : ledColor(0, 0, 0),
                  identifyMaskIndex);
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

    if (useMask)
    {
        for (uint8_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
        {
            windowOff(n); // folds stats + clears that window's mirrors
        }
        winLastCmd = 0;
    }
    else
    {
        deactivate(); // ring off + active preset's mirrors cleared
    }
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

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ledControlInit()
{
    // Which display this board has. Read once: it is a fact about the
    // hardware the factory fitted, settled at commissioning, and re-reading
    // it per frame would put an EEPROM transaction in the render path.
    useMask = (deviceTypeEffective() == DEVICE_TYPE_STANDARD);

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

void ledControlShowDemoFrame(uint16_t phase)
{
    if (useMask)
    {
        maskShowRainbow(phase);
    }
    else
    {
        ledShowRainbowRipple(phase);
    }
}

void ledControlPersistStats()
{
    // Close the running interval(s) into the accumulators first, so a flush
    // right before a reset captures the in-flight on-time too.
    if (useMask)
    {
        for (uint8_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
        {
            windowFoldOnTime(n, true); // lit intervals keep running
        }
    }
    else if (activePreset != 0 && onSinceMs != 0)
    {
        uint32_t nowMs = millis();
        uint32_t deltaMs = (nowMs - onSinceMs) + onTimeMsFrac[activePreset - 1];
        statsAddOnTime(activePreset, deltaMs / 1000);
        onTimeMsFrac[activePreset - 1] = (uint16_t)(deltaMs % 1000);
        onSinceMs = nowMs;
    }
    statsPersistIfChanged();
}

void ledControlClearStats()
{
    memset(onTimeMsFrac, 0, sizeof(onTimeMsFrac));
    uint32_t nowMs = millis();
    if (onSinceMs != 0)
    {
        onSinceMs = nowMs; // restart the in-flight interval from zero
    }
    for (uint8_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        if (winOnSinceMs[n - 1] != 0)
        {
            winOnSinceMs[n - 1] = nowMs; // same restart, per window
        }
    }
    statsClearUsage();
}

void ledControlConfirmBlink()
{
    // The press-acknowledge is the identify overlay with a short window:
    // same non-destructive machinery, so the ring returns to whatever the
    // preset engine says when the blink ends. Blinks in the color of the
    // preset lit AT THE MOMENT OF THE PRESS — captured here, because the
    // master may clear that preset mid-blink (it usually does; that is the
    // confirm loop working). No preset lit -> identify's white.
    const uint8_t w = IDENTIFY_WHITE_LEVEL;
    if (useMask)
    {
        // Exactly one window lit -> point the ack at it in its color. Any
        // other state (none lit, or several people's windows at once)
        // blinks the whole mask white: singling one out would point at the
        // wrong person's slot.
        uint8_t only = 0;
        if (winLitMask != 0 && (winLitMask & (uint8_t)(winLitMask - 1)) == 0)
        {
            for (uint8_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
            {
                if (winLitMask == (1u << (n - 1)))
                {
                    only = n;
                }
            }
        }
        identifyStart(CONFIRM_BLINK_MS,
                      only != 0 ? presetColorOf(only) : ledColor(w, w, w),
                      only);
        return;
    }
    identifyStart(CONFIRM_BLINK_MS,
                  activePreset != 0 ? presetColorOf(activePreset)
                                    : ledColor(w, w, w),
                  activePreset);
}

bool ledControlChannelOn()
{
    return useMask ? (winLitMask != 0) : (activePreset != 0);
}

bool ledControlEnableCoilOn(uint16_t coil)
{
    if (useMask)
    {
        for (uint8_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
        {
            if ((winLitMask & (1u << (n - 1))) && mbCoilLedEnable(n) == coil)
            {
                return true;
            }
        }
        return false;
    }
    // Ring: exactly the comparison latch_control used to make inline.
    return activePreset != 0 && mbCoilLedEnable(activePreset) == coil;
}

uint8_t ledControlActivePreset()
{
    if (!useMask)
    {
        return activePreset;
    }
    // Mask boards, reg 11: with a single lit window this reads exactly like
    // the ring (the window in use); with several, the last one commanded on
    // while it is still lit, else the lowest lit. The complete truth is the
    // reg-61 bitmask — this register stays for masters that read one value.
    if (winLitMask == 0)
    {
        return 0;
    }
    if (winLastCmd != 0 && (winLitMask & (1u << (winLastCmd - 1))))
    {
        return winLastCmd;
    }
    for (uint8_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        if (winLitMask & (1u << (n - 1)))
        {
            return n;
        }
    }
    return 0;
}

uint8_t ledControlLitWindows()
{
    if (useMask)
    {
        return winLitMask;
    }
    return (activePreset != 0) ? (uint8_t)(1u << (activePreset - 1)) : 0;
}

void ledControlTick(uint32_t now)
{
    // Enforce max-on-time (0 = unlimited). Per window on mask boards — pick
    // confirmation lives on the server's tablet, so this timeout is the only
    // thing that puts a forgotten window out. Single preset on ring boards,
    // unchanged.
    if (useMask)
    {
        for (uint8_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
        {
            if ((winLitMask & (1u << (n - 1))) == 0 || winOnSinceMs[n - 1] == 0)
            {
                continue;
            }
            uint16_t maxOnTimeS = mbRegRead(mbRegLedBase(n) + 4);
            if (maxOnTimeS > 0 && now - winOnSinceMs[n - 1] > (uint32_t)maxOnTimeS * 1000)
            {
                windowOff(n);
            }
        }
    }
    else if (activePreset != 0 && onSinceMs != 0)
    {
        uint16_t maxOnTimeS = mbRegRead(mbRegLedBase(activePreset) + 4);
        if (maxOnTimeS > 0 && now - onSinceMs > (uint32_t)maxOnTimeS * 1000)
        {
            deactivate(); // ring off + coil mirrors cleared (display state untouched)
        }
    }

    // Publish statistics: legacy clamped 200-281 + Statistics-v2 400-451.
    statsPublishRegisters(now);

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
