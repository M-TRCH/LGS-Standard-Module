/*  @file bringup/mask_test.cpp
 *  @brief Stand-alone bring-up for the LED-8-Index mask (Type 10 STANDARD).
 *
 *  Build and flash with:  pio run -e LGS_MASK_TEST -t upload
 *
 *  A Standard board has no OLED and no big button, so the only way to see
 *  whether its 8 pixels are alive — and whether they are in the order a
 *  person reads them — is to drive them and look. This image does nothing
 *  else: no Modbus, no EEPROM, no latch. Flash the real firmware back when
 *  the mask has been signed off.
 *
 *  The sequence, repeating, so it can be left running on a bench:
 *    1. all 8 white      — every pixel alive, and the RGBW white die works
 *    2. red, green, blue — all three colour channels on every pixel
 *    3. index 1..8 walk  — THE point of this test: window n must light in
 *                          reading order. If the wrong window lights, the
 *                          part is fine and kIndexToChain in led_mask.cpp
 *                          is what describes this board's data routing.
 *
 *  Brightness is deliberately held at a third: eight SK6812MINI at full
 *  white is roughly half an amp, which is not what a bench supply or a
 *  freshly soldered board should meet first.
 */
#include <Arduino.h>

#include "board.h"
#include "drivers/led_mask.h"

namespace {

constexpr uint8_t LVL = 0x40;           // ~25%, see the note above
constexpr uint8_t LVL_ONE = 0x80;       // one pixel at a time may run brighter

constexpr uint16_t HOLD_ALL_MS = 800;
constexpr uint16_t HOLD_STEP_MS = 600;
constexpr uint16_t GAP_MS = 300;

constexpr uint32_t WHITE = ((uint32_t)LVL << 16) | ((uint32_t)LVL << 8) | LVL;
constexpr uint32_t RED   = (uint32_t)LVL << 16;
constexpr uint32_t GREEN = (uint32_t)LVL << 8;
constexpr uint32_t BLUE  = (uint32_t)LVL;
constexpr uint32_t WALK  = (uint32_t)LVL_ONE << 8;   // green: easiest to see

/*  The app arms a 4 s independent watchdog and, once started, the IWDG
 *  survives a software reset — so an image that never feeds it would reset
 *  every four seconds on any board that has run the real firmware. Feeding
 *  it costs one register write and is harmless when it was never started.
 */
void kickWatchdog()
{
    IWDG->KR = 0x0000AAAAu;
}

void hold(uint16_t ms)
{
    const uint32_t until = millis() + ms;
    while ((int32_t)(millis() - until) < 0)
    {
        kickWatchdog();
        delay(10);
    }
}

}  // namespace

void setup()
{
    pinMode(HW_LED_BUILTIN_PIN, OUTPUT);
    maskInit();
}

void loop()
{
    digitalWrite(HW_LED_BUILTIN_PIN, HIGH);     // a heartbeat for "running"

    maskSetAll(WHITE);  hold(HOLD_ALL_MS);
    maskSetAll(RED);    hold(HOLD_ALL_MS);
    maskSetAll(GREEN);  hold(HOLD_ALL_MS);
    maskSetAll(BLUE);   hold(HOLD_ALL_MS);
    maskOff();          hold(GAP_MS);

    digitalWrite(HW_LED_BUILTIN_PIN, LOW);

    for (uint8_t index = 1; index <= HW_LED_MASK_PIXEL_COUNT; index++)
    {
        maskShowIndex(index, WALK);
        hold(HOLD_STEP_MS);
    }
    maskOff();
    hold(GAP_MS * 2);
}
