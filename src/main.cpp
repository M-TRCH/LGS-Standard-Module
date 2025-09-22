
#include <Arduino.h>
#include "config.h"
#include "system.h"
#include "led.h"
#include "modbus_utils.h"
#include "eeprom_utils.h"
 
void setup() 
{
    #ifdef SYSTEM_H
        sysInit();  // Initialize system
    #endif

    #ifdef EEPROM_UTILS_H
        // clearEeprom(true);   // Uncomment to clear EEPROM for debugging  
        handleFirstBoot();      // Handle first boot scenario
        loadEepromConfig();     // Load configuration from EEPROM
        PRINT(DEBUG_BASIC, F("EEPROM loaded\n"));
    #endif

    #ifdef LED_H
        ledInit();  // Initialize LEDs
    #endif

    #ifdef MODBUS_UTILS_H
        PRINT(DEBUG_BASIC, "Modbus ID: " + String(eepromConfig.identifier) + "\n");
        modbusInit(eepromConfig.identifier);    // Initialize Modbus
        eeprom2modbusMapping();
    #endif
}

void loop() 
{  
#ifdef SYSTEM_H
    // Blink the run LED
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink >= LED_BLINK_MS) 
    {
        lastBlink = millis();
        run_led_state = !run_led_state;
        digitalWrite(LED_RUN_PIN, run_led_state);
    }
#endif

    // Poll Modbus server for requests
    if(RTUServer.poll()) 
    {   
        // Write data to EEPROM
        if (RTUServer.coilRead(MB_COIL_WRITE_TO_EEPROM)) 
        {
            PRINT(DEBUG_BASIC, F("Saving configuration to EEPROM via Modbus\n"));
            modbus2eepromMapping();
            NVIC_SystemReset();         // Perform software reset
        }

        // Apply factory reset
        if (RTUServer.coilRead(MB_COIL_FACTORY_RESET)) 
        {
            if (RTUServer.coilRead(MB_COIL_APPLY_FACTORY_RESET_EXCEPT_ID)) 
            {
                PRINT(DEBUG_BASIC, F("Factory Reset (Except ID) via Modbus\n"));
                eepromConfig.isFirstBootExceptID = true;    // Clear the flag
                saveEepromConfig();                         // Save to EEPROM if changed
                NVIC_SystemReset();                         // Perform software reset
            }

            if (RTUServer.coilRead(MB_COIL_APPLY_FACTORY_RESET_ALL_DATA)) 
            {
                PRINT(DEBUG_BASIC, F("Factory Reset via Modbus\n"));
                eepromConfig.isFirstBoot = true;        // Clear the flag
                saveEepromConfig();                     // Save to EEPROM if changed
                NVIC_SystemReset();                     // Perform software reset
            }
        }

        // Apply LED state changes
        for (int i = 0; i < LED_NUM; i++) 
        {
            int led_state = RTUServer.coilRead(MB_COIL_LED_1_ENABLE + i);
            if (led_state != last_led_state[i]) // Check if the LED state has changed
            {
                last_led_state[i] = led_state;

                if (led_state) // If the LED state is ON
                {
                    float brightness = RTUServer.holdingRegisterRead(MB_REG_LED_1_BRIGHTNESS + i*10) / 100.0;   // Scale brightness to 0.0 - 1.0
                    leds[i]->setPixelColor(0, leds[i]->Color(
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_RED + i*10) * brightness,    // i*10 to jump to next LED's registers (E.g., 110, 120, 130...)
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_GREEN + i*10) * brightness,
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_BLUE + i*10) * brightness));
                    leds[i]->show();

                    led_counter[i]++;           // Increment counter for how many times this LED has been turned on
                    led_timer[i] = millis();    // Start timer for how long this LED has been on
                }   
                else    // If the LED state is OFF
                {
                    leds[i]->setPixelColor(0, leds[i]->Color(0, 0, 0));
                    leds[i]->show();
                
                    // Stop timer and accumulate time if it was running
                    if (led_timer[i] != 0) 
                    {
                        led_time_sum[i] += (millis() - led_timer[i]) / 1000.0; // Convert ms to seconds
                        led_timer[i] = 0; // Reset timer
                    }
                }
                
                // Log LED state change
                PRINT(DEBUG_BASIC, "LED1:" + String(led_counter[0]) + ", " + String(led_time_sum[0]) + "\t");
                PRINT(DEBUG_BASIC, "LED2:" + String(led_counter[1]) + ", " + String(led_time_sum[1]) + "\t");
                PRINT(DEBUG_BASIC, "LED3:" + String(led_counter[2]) + ", " + String(led_time_sum[2]) + "\t");
                PRINT(DEBUG_BASIC, "LED4:" + String(led_counter[3]) + ", " + String(led_time_sum[3]) + "\t");
                PRINT(DEBUG_BASIC, "LED5:" + String(led_counter[4]) + ", " + String(led_time_sum[4]) + "\t");
                PRINT(DEBUG_BASIC, "LED6:" + String(led_counter[5]) + ", " + String(led_time_sum[5]) + "\t");
                PRINT(DEBUG_BASIC, "LED7:" + String(led_counter[6]) + ", " + String(led_time_sum[6]) + "\t");
                PRINT(DEBUG_BASIC, "LED8:" + String(led_counter[7]) + ", " + String(led_time_sum[7]) + "\n");
            }
        }
    }
}
