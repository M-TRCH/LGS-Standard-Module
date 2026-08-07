#include "drivers/led_mask.h"
#include <Adafruit_NeoPixel.h>

// SK6812MINI RGBW: same 800 kHz protocol as the ring's WS2812B but four
// bytes per pixel, the fourth driving a dedicated white die.
static Adafruit_NeoPixel mask(HW_LED_MASK_PIXEL_COUNT, HW_LED_MASK_PIN,
                              NEO_GRBW + NEO_KHZ800);

/*  Colors arrive as the ring's packed RGB (0x00RRGGBB) so the preset engine
 *  stays one code path for both displays. Neutral values are re-routed to
 *  the white die: mixing R+G+B to make white on an RGBW part gives a tinted
 *  white that drifts between production lots, and burns three emitters to do
 *  what one does better. Anything with actual hue passes through untouched
 *  with W left dark, so preset colors keep their intended saturation.
 */
static uint32_t toMaskColor(uint32_t rgb)
{
    const uint8_t r = (uint8_t)(rgb >> 16);
    const uint8_t g = (uint8_t)(rgb >> 8);
    const uint8_t b = (uint8_t)rgb;
    if (r == g && g == b)
    {
        return (uint32_t)r << 24;       // W only; r == 0 is simply "off"
    }
    return rgb;
}

void maskInit()
{
    mask.begin();       // drives the data pin OUTPUT LOW, defining the line
    mask.clear();
    // Twice, for the same reason as the ring: the first show() after boot
    // can glitch the leading pixel's bit timing on STM32.
    mask.show();
    mask.show();
}

// Index as read off the front of the mask (1-8) -> position in the data
// chain. The chain does not run in reading order; the parts are placed for
// the mask's layout and the data line snakes between them. Measured on the
// board (commanding chain position 1 lights the 4th window, and so on), and
// it lives here because it describes this accessory's wiring — the preset
// engine should keep thinking in the numbers a person sees.
static const uint8_t kIndexToChain[HW_LED_MASK_PIXEL_COUNT] =
    { 0, 3, 4, 7, 1, 2, 5, 6 };

void maskShowIndex(uint8_t index, uint32_t color)
{
    mask.clear();
    if (index >= 1 && index <= HW_LED_MASK_PIXEL_COUNT)
    {
        mask.setPixelColor(kIndexToChain[index - 1], toMaskColor(color));
    }
    mask.show();
}

void maskSetAll(uint32_t color)
{
    const uint32_t c = toMaskColor(color);
    for (uint16_t pixel = 0; pixel < HW_LED_MASK_PIXEL_COUNT; pixel++)
    {
        mask.setPixelColor(pixel, c);
    }
    mask.show();
}

void maskOff()
{
    mask.clear();
    mask.show();
}
