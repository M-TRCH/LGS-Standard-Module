#include "app/display_control.h"
#include "drivers/oled.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

namespace {

bool ownScreen = false;   // RUN mode + OLED present: this module may draw
bool displayOn = false;   // logical display state (mirrors coil 1010)
bool otaScreen = false;   // the firmware-update face is up, outside coil 1010

void render()
{
    if (ownScreen)
    {
        oledPrintLargeNumber((uint8_t)mbRegRead(MB_REG_SET_NUM_DISPLAY));
    }
}

// Set the number on the display (reg 60). R5.0 renders 0-99: out-of-range
// writes clamp to 99 and the register reflects the clamped value (mbRegWrite
// syncs the CHANGE shadow, so the write-back cannot re-fire this handler).
// While the display is enabled the new number renders immediately — the
// handler runs after the Modbus response has been flushed, so the ~20 ms
// OLED transfer only delays the next poll.
void onSetNumDisplayChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    if (value > 99)
    {
        mbRegWrite(MB_REG_SET_NUM_DISPLAY, 99);
    }
    if (displayOn)
    {
        render();
    }
}

// Enable Display (coil 1010): 1 = show the reg-60 number, 0 = blank screen
void onDisplayEnableChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    displayControlSetEnabled(value != 0);
}

} // namespace

void displayControlInit(bool runScreenOwner)
{
    ownScreen = runScreenOwner;
    if (ownScreen)
    {
        // Wipe whatever the boot mode selector left on screen; from here on
        // the RUN screen belongs to this module (blank until coil 1010).
        oledClear();
    }

    mbRegisterHandler(MB_WATCH_REG_CHANGE, MB_REG_SET_NUM_DISPLAY, onSetNumDisplayChange);
    mbRegisterHandler(MB_WATCH_COIL_CHANGE, MB_COIL_DISPLAY_ENABLE, onDisplayEnableChange);
}

void displayControlShowNumber(uint16_t value)
{
    if (value > 99)
    {
        value = 99;
    }
    mbRegWrite(MB_REG_SET_NUM_DISPLAY, value); // shadow-synced: handler won't re-fire
    if (displayOn)
    {
        render();
    }
}

void displayControlShowOta(uint8_t percent, uint16_t done, uint16_t total)
{
    if (!ownScreen)
    {
        return;
    }
    otaScreen = true;
    oledPrintOtaProgress(percent, done, total);
}

void displayControlSetEnabled(bool on)
{
    // Mirror the enable coil first (idempotent; the shadow sync keeps this
    // from re-firing onDisplayEnableChange).
    mbCoilWrite(MB_COIL_DISPLAY_ENABLE, on);

    // Switching off always runs when an update face is up, even though the
    // logical state was already off: something has to wipe that screen when
    // the transfer ends, and it never went through this flag on the way in.
    if (on == displayOn && !(otaScreen && !on))
    {
        return;
    }
    displayOn = on;
    otaScreen = false;
    if (on)
    {
        render();
    }
    else if (ownScreen)
    {
        oledClear();
    }
}
