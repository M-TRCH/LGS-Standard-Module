
#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "system.h"

#define LED_NUM_PER_STRIP     1     // Number of LEDs per strip
#define LED_NUM               8     // Total number of LEDs
#define DEFAULT_LED_POWER     20    // Default LED power percentage (0-100)
#define DEFAULT_LED_PWM       (DEFAULT_LED_POWER / 100.0 * 255.0)

extern Adafruit_NeoPixel null_led, led1, led2, led3, led4, led5, led6, led7, led8;
extern Adafruit_NeoPixel *leds[];
extern float default_color[17][3];          // Array to hold default RGB values for each LED
extern bool last_led_state[LED_NUM+1];      // Array to hold last known state (on/off) for each LED

void ledInit();

#endif