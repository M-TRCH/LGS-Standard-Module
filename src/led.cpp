#include "led.h"

Adafruit_NeoPixel null_led;
Adafruit_NeoPixel led1(LED_NUM_PER_STRIP, LED1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led2(LED_NUM_PER_STRIP, LED2_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led3(LED_NUM_PER_STRIP, LED3_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led4(LED4_NUM_PER_STRIP, LED4_PIN, NEO_GRB + NEO_KHZ800);
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
uint32_t led_counter[LED_NUM] = {0};    // Counter for each LED having been turned on
uint32_t led_timer[LED_NUM] = {0};      // Timer for each LED having been turned on
float led_time_sum[LED_NUM] = {0};      // Total time each LED has been on (in seconds)

void ledInit() 
{   
    LOG_INFO_LED(F("[LED] Initializing LED strips...\n"));
    // Initialize LED strips
    for (int i = 0; i < LED_NUM; i++) 
    {
        leds[i]->begin();
        leds[i]->setPixelColor(0, leds[i]->Color(0,0,0));
        leds[i]->show(); // Initialize all pixels to 'off'
    }
    LOG_INFO_LED(F("[LED] LED initialization complete\n"));
}

void printLedStatus() 
{
    LOG_INFO_LED(F("[LED] Status Report:\n"));
    for (int i = 0; i < LED_NUM; i++) 
    {
        LOG_INFO_LED("  L" + String(i+1) + ": State=" + String(last_led_state[i] ? "ON" : "OFF") +
              ", Count=" + String(led_counter[i]) +
              ", Time=" + String(led_time_sum[i], 2) + "s\n");
    }
}

void testLed4(uint8_t pwm) 
{
    LOG_INFO_LED(F("[LED] Testing LED4 strip with 20 LEDs (PWM="));
    LOG_INFO_LED(String(pwm) + ")...\n");
    
    // Test 1: Rainbow cycle
    LOG_INFO_LED(F("[LED] Test 1: Rainbow cycle\n"));
    for (int j = 0; j < 256; j++) 
    {
        for (int i = 0; i < LED4_NUM_PER_STRIP; i++) 
        {
            int pixelHue = (i * 65536L / LED4_NUM_PER_STRIP) + j * 256;
            led4.setPixelColor(i, led4.gamma32(led4.ColorHSV(pixelHue)));
        }
        led4.show();
        delay(10);
    }
    
    /*
    // Test 2: Color wipe - Red
    LOG_INFO_LED(F("[LED] Test 2: Red color wipe\n"));
    for (int i = 0; i < LED4_NUM_PER_STRIP; i++) 
    {
        led4.setPixelColor(i, led4.Color(pwm, 0, 0));
        led4.show();
        delay(50);
    }
    delay(500);
    
    // Test 3: Color wipe - Green
    LOG_INFO_LED(F("[LED] Test 3: Green color wipe\n"));
    for (int i = 0; i < LED4_NUM_PER_STRIP; i++) 
    {
        led4.setPixelColor(i, led4.Color(0, pwm, 0));
        led4.show();
        delay(50);
    }
    delay(500);
    
    // Test 4: Color wipe - Blue
    LOG_INFO_LED(F("[LED] Test 4: Blue color wipe\n"));
    for (int i = 0; i < LED4_NUM_PER_STRIP; i++) 
    {
        led4.setPixelColor(i, led4.Color(0, 0, pwm));
        led4.show();
        delay(50);
    }
    delay(500);
   
    // Test 5: All LEDs white
    LOG_INFO_LED(F("[LED] Test 5: All LEDs white\n"));
    for (int i = 0; i < LED4_NUM_PER_STRIP; i++) 
    {
        led4.setPixelColor(i, led4.Color(pwm, pwm, pwm));
    }
    led4.show();
    delay(1000);
    
    // Turn off all LEDs
    LOG_INFO_LED(F("[LED] Test complete - turning off LED4\n"));
    for (int i = 0; i < LED4_NUM_PER_STRIP; i++) 
    {
        led4.setPixelColor(i, led4.Color(0, 0, 0));
    }
    */
    
    led4.show();
}