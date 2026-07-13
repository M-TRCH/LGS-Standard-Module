#include "app/led_control.h"
#include "config.h"
#include "app/latch_control.h"
#include "drivers/led_ring.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

// ---------------------------------------------------------------------------
// Channel state (single logical channel driving the full ring)
// ---------------------------------------------------------------------------

namespace {

bool channelOn = false;         // last known on/off state
uint32_t onCount = 0;           // times the channel has been turned on
uint32_t onSinceMs = 0;         // millis() when it was last turned on (0 = off)
uint32_t onTimeSumMs = 0;       // accumulated on-time in milliseconds

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

// Apply the configured RGB color (from the Modbus registers), scaled by the
// configured brightness.
void applyConfiguredColor()
{
    uint16_t brightness = mbRegRead(MB_REG_LED_1_BRIGHTNESS);
    if (brightness > 100)
    {
        brightness = 100;
    }
    ledSetAllPixels(0, ledColor(
        scaledComponent(mbRegRead(MB_REG_LED_1_RED), brightness),
        scaledComponent(mbRegRead(MB_REG_LED_1_GREEN), brightness),
        scaledComponent(mbRegRead(MB_REG_LED_1_BLUE), brightness)));
}

void markChannelOn()
{
    if (!channelOn)
    {
        channelOn = true;
        onCount++;
        onSinceMs = millis();
    }
}

void markChannelOff()
{
    channelOn = false;
    if (onSinceMs != 0)
    {
        onTimeSumMs += millis() - onSinceMs;
        onSinceMs = 0;
    }
}

// --- Modbus handlers ---

// Enable coil (1001): change-triggered on/off
void onLedEnableChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    if (value)
    {
        applyConfiguredColor();
        markChannelOn();
    }
    else
    {
        ledSetAllPixels(0, ledColor(0, 0, 0));
        markChannelOff();
    }
}

// LED-latch coil (1021): turn the LED on and pulse the latch in one command
void onLedLatchCommand(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;

    if (latchBusyWith(MB_COIL_LED_1_LATCH))
    {
        return; // request in flight: coil stays set until the pulse resolves
    }

    applyConfiguredColor();
    markChannelOn();

    // Hand the unlock to the latch state machine; on completion it clears
    // this coil and syncs the enable coil. When busy (another request or
    // cooldown) the LED still turns on — same as the original reject path —
    // but the coils resolve immediately so the command is not retried.
    if (!latchRequestUnlock(LATCH_PULSE_MS, MB_COIL_LED_1_LATCH, true))
    {
        mbCoilWrite(MB_COIL_LED_1_LATCH, false);
        mbCoilWrite(MB_COIL_LED_1_ENABLE, true);
    }
}

// Global brightness (190): fan out to the LED brightness register.
// Persist-style semantics: no re-apply to already-lit pixels (the value
// takes effect at the next enable edge), matching the original firmware.
void onGlobalBrightnessChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    if (value <= 100)
    {
        mbRegWrite(MB_REG_LED_1_BRIGHTNESS, value);
    }
}

// Global max on-time (194): fan out to the LED max-on-time register.
void onGlobalMaxOnTimeChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    mbRegWrite(MB_REG_LED_1_MAX_ON_TIME, value);
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
    mbRegisterHandler(MB_WATCH_COIL_CHANGE, MB_COIL_LED_1_ENABLE, onLedEnableChange);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_LED_1_LATCH, onLedLatchCommand);
    mbRegisterHandler(MB_WATCH_REG_CHANGE, MB_REG_GLOBAL_BRIGHTNESS, onGlobalBrightnessChange);
    mbRegisterHandler(MB_WATCH_REG_CHANGE, MB_REG_GLOBAL_MAX_ON_TIME, onGlobalMaxOnTimeChange);
}

void ledControlTick(uint32_t now)
{
    // Enforce the max-on-time limit (0 = unlimited)
    uint16_t maxOnTimeS = mbRegRead(MB_REG_LED_1_MAX_ON_TIME);
    if (onSinceMs != 0 && maxOnTimeS > 0)
    {
        if (now - onSinceMs > (uint32_t)maxOnTimeS * 1000)
        {
            ledSetAllPixels(0, ledColor(0, 0, 0));
            markChannelOff();
            mbCoilWrite(MB_COIL_LED_1_ENABLE, false); // reflect the forced off-state
        }
    }

    // Publish statistics (clamped to uint16 range, seconds truncated)
    uint16_t countValue = clampToU16(onCount);
    uint16_t timeValue = clampToU16(onTimeSumMs / 1000);

    mbRegWrite(MB_REG_LED_1_ON_COUNTER, countValue);
    mbRegWrite(MB_REG_LED_1_ON_TIME, timeValue);

    // Totals equal the single channel's numbers on this board
    mbRegWrite(MB_REG_TOTAL_LED_ON_CNT, countValue);
    mbRegWrite(MB_REG_TOTAL_LED_ON_TIME, timeValue);
}
