#ifndef HW_LED_H
#define HW_LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "system.h"

/*  @file hw/led.h
 *  @brief Addressable LED driver (WS2812B 16-pixel ring on PA8).
 *
 *  The physical ring is exposed to the application as LED_NUM logical groups so
 *  the existing Modbus register/coil layout remains unchanged.
 */

#define LED_NUM               8     // Total logical LED groups exposed to Modbus
#define LED_PIXELS_PER_GROUP  2     // 16-pixel ring split into 8 logical groups
#define LED_RING_PIXEL_COUNT  HW_LED_RING_PIXEL_COUNT
#define DEFAULT_LED_POWER     20    // Default LED power percentage (0-100)
#define DEFAULT_LED_PWM       (DEFAULT_LED_POWER / 100.0 * 255.0)

extern Adafruit_NeoPixel ledRing;
extern float default_color[17][3];      // Array to hold default RGB values for each LED
extern bool last_led_state[LED_NUM];    // Array to hold last known state (on/off) for each LED
extern uint32_t led_counter[LED_NUM];   // Counter for each LED having been turned on
extern uint32_t led_timer[LED_NUM];     // Timer for each LED having been turned on
extern float led_time_sum[LED_NUM];     // Total time each LED has been on (in seconds)

/*  @brief Pack an RGB triplet into the ring's native color format. */
uint32_t ledColor(uint8_t red, uint8_t green, uint8_t blue);

/*  @brief Initialize the LED ring. */
void ledInit();

/*  @brief Set every pixel of a logical LED group to a color. */
void ledSetAllPixels(int ledIndex, uint32_t color);

/*  @brief Log the current state of all logical LED groups. */
void printLedStatus();

#endif // HW_LED_H
