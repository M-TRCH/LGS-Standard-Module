#include "led.h"

Adafruit_NeoPixel null_led;
Adafruit_NeoPixel led1(LED_NUM_PER_STRIP, LED1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led2(LED_NUM_PER_STRIP, LED2_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led3(LED_NUM_PER_STRIP, LED3_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led4(LED_NUM_PER_STRIP, LED4_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led5(LED_NUM_PER_STRIP, LED5_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led6(LED_NUM_PER_STRIP, LED6_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led7(LED_NUM_PER_STRIP, LED7_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led8(LED_NUM_PER_STRIP, LED8_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel *leds[] = {&led1, &led2, &led3, &led4, &led5, &led6, &led7, &led8};

float default_color[17][3] = 
{
    {0, 0, 0},                                              // Off
    {DEFAULT_LED_PWM, 0, 0},                                // Red
    {0, DEFAULT_LED_PWM, 0},                                // Green
    {0, 0, DEFAULT_LED_PWM},                                // Blue
    {DEFAULT_LED_PWM, DEFAULT_LED_PWM, 0},                  // Yellow
    {0, DEFAULT_LED_PWM, DEFAULT_LED_PWM},                  // Cyan
    {DEFAULT_LED_PWM, 0, DEFAULT_LED_PWM},                  // Magenta
    {DEFAULT_LED_PWM, DEFAULT_LED_PWM/2, 0},                // Orange
    {DEFAULT_LED_PWM, DEFAULT_LED_PWM, DEFAULT_LED_PWM},    // White
    {DEFAULT_LED_PWM/2, 0, DEFAULT_LED_PWM},                // Violet
    {0, DEFAULT_LED_PWM/2, DEFAULT_LED_PWM},                // Azure
    {DEFAULT_LED_PWM/2, DEFAULT_LED_PWM, 0},                // Chartreuse
    {DEFAULT_LED_PWM, 0, DEFAULT_LED_PWM/2},                // Rose
    {0, DEFAULT_LED_PWM, DEFAULT_LED_PWM/2},                // Spring Green
    {DEFAULT_LED_PWM/2, 0, 0},                              // Maroon
    {0, DEFAULT_LED_PWM/2, 0},                              // Dark Green
    {0, 0, DEFAULT_LED_PWM/2}                               // Navy
};

bool last_led_state[LED_NUM] = {false, false, false, false, false, false, false, false};

void ledInit() 
{   
    // Initialize LED strips
    for (int i = 0; i < LED_NUM; i++) 
    {
        leds[i]->begin();
        leds[i]->setPixelColor(0, leds[i]->Color(0,0,0));
        leds[i]->show(); // Initialize all pixels to 'off'
    }
}