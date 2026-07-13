#include "app/display_control.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

namespace {

// Set the numbers on the display (reg 60, 0-9999)
void onSetNumDisplayChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    // TODO: render the value on the OLED (drivers/oled currently draws
    // two digits; 0-9999 needs a four-digit layout).
}

// Enable Display (coil 1010)
void onDisplayEnableChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    // TODO: turn the OLED content on/off.
}

// Enable Light 1 and Display (coil 1011)
void onLedDisplayChange(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    // TODO: combined light + display command.
}

} // namespace

void displayControlInit()
{
    mbRegisterHandler(MB_WATCH_REG_CHANGE, MB_REG_SET_NUM_DISPLAY, onSetNumDisplayChange);
    mbRegisterHandler(MB_WATCH_COIL_CHANGE, MB_COIL_DISPLAY_ENABLE, onDisplayEnableChange);
    mbRegisterHandler(MB_WATCH_COIL_CHANGE, MB_COIL_LED_1_DISPLAY, onLedDisplayChange);
}
