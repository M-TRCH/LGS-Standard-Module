#include "app/display_control.h"
#include "drivers/oled.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

namespace {

bool ownScreen = false;   // RUN mode + OLED present: this module may draw
bool displayOn = false;   // logical display state (mirrors coil 1010)

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

void displayControlSetEnabled(bool on)
{
    // Mirror the enable coil first (idempotent; the shadow sync keeps this
    // from re-firing onDisplayEnableChange).
    mbCoilWrite(MB_COIL_DISPLAY_ENABLE, on);

    if (on == displayOn)
    {
        return;
    }
    displayOn = on;
    if (on)
    {
        render();
    }
    else if (ownScreen)
    {
        oledClear();
    }
}
