#include "drivers/led_ring.h"

static Adafruit_NeoPixel ledRing(LED_RING_PIXEL_COUNT, HW_LED_RING_PIN, NEO_GRB + NEO_KHZ800);

uint32_t ledColor(uint8_t red, uint8_t green, uint8_t blue)
{
    return ledRing.Color(red, green, blue);
}

void ledSetAllPixels(int ledIndex, uint32_t color)
{
    int startPixel = ledIndex * LED_PIXELS_PER_GROUP;
    int endPixel = startPixel + LED_PIXELS_PER_GROUP;

    for (int pixel = startPixel; pixel < endPixel && pixel < LED_RING_PIXEL_COUNT; pixel++)
    {
        ledRing.setPixelColor(pixel, color);
    }
    ledRing.show();
}

void ledShowRainbowRipple(uint16_t phase)
{
    for (int pixel = 0; pixel < LED_RING_PIXEL_COUNT; pixel++)
    {
        uint16_t hue = (phase * 192U) + (pixel * (65535U / LED_RING_PIXEL_COUNT));
        uint8_t wave = (uint8_t)((phase * 6U) + (pixel * 14U));
        uint8_t triangle = (wave < 128U) ? (wave << 1) : ((255U - wave) << 1);
        uint8_t eased = (uint8_t)(((uint16_t)triangle * (uint16_t)triangle) / 255U);
        uint8_t brightness = 24U + (uint8_t)(((uint16_t)eased * 200U) / 255U);

        uint32_t color = ledRing.gamma32(ledRing.ColorHSV(hue, 255, brightness));
        ledRing.setPixelColor(pixel, color);
    }

    ledRing.show();
}

void ledInit()
{
    ledRing.begin();
    ledRing.clear();
    ledRing.show();

    for (int i = 0; i < LED_NUM; i++)
    {
        ledSetAllPixels(i, ledColor(0, 0, 0));
    }
}
