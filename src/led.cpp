
#include "led.h"

Adafruit_NeoPixel led1(LED_NUM_PER_STRIP, LED1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led2(LED_NUM_PER_STRIP, LED2_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led3(LED_NUM_PER_STRIP, LED3_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led4(LED_NUM_PER_STRIP, LED4_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led5(LED_NUM_PER_STRIP, LED5_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led6(LED_NUM_PER_STRIP, LED6_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led7(LED_NUM_PER_STRIP, LED7_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led8(LED_NUM_PER_STRIP, LED8_PIN, NEO_GRB + NEO_KHZ800);

Adafruit_NeoPixel *leds[] = {&led1, &led2, &led3, &led4, &led5, &led6, &led7, &led8};

void ledInit() 
{
    for (int i = 0; i < LED_NUM; i++) 
    {
        leds[i]->begin();
    }
}