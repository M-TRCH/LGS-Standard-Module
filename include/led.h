
#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "system.h"

#define LED_NUM_PER_STRIP     1     // Number of LEDs per strip
#define LED_NUM               8     // Total number of LEDs

extern Adafruit_NeoPixel led1, led2, led3, led4, led5, led6, led7, led8;
extern Adafruit_NeoPixel *leds[];

void ledInit();

#endif