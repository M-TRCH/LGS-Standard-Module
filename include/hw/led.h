#ifndef HW_LED_H
#define HW_LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "system.h"

/*  @file hw/led.h
 *  @brief Addressable LED driver (WS2812B 16-pixel ring on PA8).
 *
 *  The physical ring is exposed to Modbus as a single logical LED channel
 *  (LED_1), so every update applies to all 16 pixels together. Legacy
 *  LED_2..LED_8 Modbus addresses are kept in the map for backward
 *  compatibility only and are ignored by the firmware.
 */

#define LED_NUM               1                   // Only LED_1 is exposed to Modbus
#define LED_PIXELS_PER_GROUP  LED_RING_PIXEL_COUNT // LED_1 controls the full 16-pixel ring
#define LED_RING_PIXEL_COUNT  HW_LED_RING_PIXEL_COUNT

extern Adafruit_NeoPixel ledRing;
extern bool last_led_state[LED_NUM];    // Array to hold last known state (on/off) for each LED
extern uint32_t led_counter[LED_NUM];   // Counter for each LED having been turned on
extern uint32_t led_timer[LED_NUM];     // Timer for each LED having been turned on
extern float led_time_sum[LED_NUM];     // Total time each LED has been on (in seconds)

/*  @brief Pack an RGB triplet into the ring's native color format. */
uint32_t ledColor(uint8_t red, uint8_t green, uint8_t blue);

/*  @brief Initialize the LED ring. */
void ledInit();

/*  @brief Set the full LED ring (LED_1) to a color. */
void ledSetAllPixels(int ledIndex, uint32_t color);

/*  @brief Render a moving rainbow ripple across the full LED ring.
 *
 *  @param phase Animation phase, incremented by the caller over time
 */
void ledShowRainbowRipple(uint16_t phase);

#endif // HW_LED_H
