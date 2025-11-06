
// Reference Documentation: LGS-Control-Table-Updated-xx1125.pdf

#include <Arduino.h>
#include "led.h"
#include "eeprom_utils.h"
#include "modbus_utils.h"

void setup() 
{
#ifdef SYSTEM_H
    sysInit(LOG_DEBUG);  // Initialize system
#endif

#ifdef EEPROM_UTILS_H
    // clearEeprom(true);   // Uncomment to clear EEPROM for debugging  
    eepromInit();        // Initialize EEPROM and load configuration
#endif

#ifdef LED_H
    ledInit();  // Initialize LEDs
#endif

#ifdef MODBUS_UTILS_H
    // Initialize Modbus server with ID from EEPROM, default ID (247) or special ID (246) for SET_ID mode
    modbusInit(functionMode == FUNC_SW_SET_ID ? DEFAULT_IDENTIFIER-1 : eepromConfig.identifier);    
    eeprom2modbusMapping(); // Map EEPROM config to Modbus registers
#endif
}

void loop() 
{
    // For testing purposes: read temperature from STS4x sensor
    // float temperature;
    // static uint32_t lastTempReadSec = 0;
    // static uint32_t lastTempRead = 0;
    // if (millis() - lastTempReadSec >= 1000) // Read every second
    // {
    //     lastTempReadSec = millis();
    //     LOG_DEBUG_SYS(".");
    //     if (millis() - lastTempRead >= 60000)
    //     {
    //         lastTempRead = millis();
    //         sts4x.measureHighPrecision(temperature);
    //         LOG_DEBUG_SYS("Temperature: " + String(temperature, 2) + " °C\n");
    //     }    
    // }
    // LOG_DEBUG_SYS("Func SW: " + String(digitalRead(FUNC_SW_PIN)) + "\n");
    
    // Demo mode: cycle through LEDs
    if (functionMode == FUNC_SW_DEMO)
    {
        if (ON_ROUTINE_BLINK_DEMO())
        {
            for (int i = 0; i < LED_NUM; i++) 
            {
                float brightness = RTUServer.holdingRegisterRead(MB_REG_LED_1_BRIGHTNESS + i*10) / 100.0;   // Scale brightness to 0.0 - 1.0
                if (!blink_demo_state)  brightness = 0.0; // Turn off LEDs when blink state is off

                leds[i]->setPixelColor(0, leds[i]->Color(
                    RTUServer.holdingRegisterRead(MB_REG_LED_1_RED + i*10) * brightness,    // i*10 to jump to next LED's registers (E.g., 110, 120, 130...)
                    RTUServer.holdingRegisterRead(MB_REG_LED_1_GREEN + i*10) * brightness,
                    RTUServer.holdingRegisterRead(MB_REG_LED_1_BLUE + i*10) * brightness));
                leds[i]->show();
            }
        }
    }
    
    // Set ID mode: blink all LEDs in red for identification
    else if (functionMode == FUNC_SW_SET_ID) 
    {
        if (ON_ROUTINE_BLINK_SET_ID())
        {
            for (int i = 0; i < LED_NUM; i++) 
            {
                leds[i]->setPixelColor(0, leds[i]->Color(0,0,(blink_set_id_state ? 204 : 0))); // Set to blue color or off (magic number)
                leds[i]->show();
            }
        }
    }
    
    // Factory reset mode: solid red on all LEDs for 5 seconds, then reset
    else if (functionMode == FUNC_SW_FACTORY_RESET)
    {
        for (int i = 0; i < LED_NUM; i++) 
        {
            leds[i]->setPixelColor(0, leds[i]->Color(204,0,0)); // Set to red color (magic number)
            leds[i]->show();
        }
        delay(5000);   // (magic number)
        LOG_INFO_SYS(F("[SYSTEM] Factory reset mode engaged.\n"));
        eepromConfig.isFirstBoot = true;        // Clear the flag
        saveEepromConfig();                     // Save to EEPROM if changed
        NVIC_SystemReset();                     // Perform software reset  
    }
    
    // Normal operation
    else if (functionMode == FUNC_SW_RUN) 
    {
        // Routine blink for run LED
        if (ON_ROUTINE_BLINK_RUN())
        {
            digitalWrite(LED_RUN_PIN, blink_run_state);
        }

        // Update LED statistics and enforce max on-time limits
        for (int i = 0; i < LED_NUM; i++)
        {
            // Check if LED is ON and has a max on-time limit set
            if (led_timer[i] != 0 && millis() - led_timer[i] > RTUServer.holdingRegisterRead(MB_REG_LED_1_MAX_ON_TIME + i*10) * 1000) // Convert seconds to ms
            {
                LOG_WARNING_LED("[LED] L" + String(i+1) + " max on-time exceeded, turning off\n");
                // Turn off the LED
                leds[i]->setPixelColor(0, leds[i]->Color(0, 0, 0));
                leds[i]->show();
                last_led_state[i] = false; // Update last known state

                // Stop timer and accumulate time
                led_time_sum[i] += (millis() - led_timer[i]) / 1000.0; // Convert ms to seconds
                led_timer[i] = 0; // Reset timer

                // Update Modbus coil to reflect LED is off
                RTUServer.coilWrite(MB_COIL_LED_1_ENABLE + i, 0);
            }

            // Update Modbus registers with current LED statistics
            RTUServer.holdingRegisterWrite(MB_REG_LED_1_ON_COUNTER + i*10, led_counter[i]);             // Update on-time count
            RTUServer.holdingRegisterWrite(MB_REG_LED_1_ON_TIME + i*10, (uint32_t)led_time_sum[i]);     // Update total on-time in seconds
        }
    }

    // Poll Modbus server for requests
    if(RTUServer.poll()) 
    {   
        // Write data to EEPROM (Addr.503)
        if (RTUServer.coilRead(MB_COIL_WRITE_TO_EEPROM)) 
        {
            LOG_INFO_MODBUS(F("[MODBUS] Saving configuration to EEPROM\n"));
            modbus2eepromMapping();
            NVIC_SystemReset();         // Perform software reset
        }

        // Apply factory reset (Addr.500)
        if (RTUServer.coilRead(MB_COIL_FACTORY_RESET)) 
        {
            // Address 501: Factory reset except ID
            if (RTUServer.coilRead(MB_COIL_APPLY_FACTORY_RESET_EXCEPT_ID)) 
            {
                LOG_INFO_MODBUS(F("[MODBUS] Factory reset (except ID) requested\n"));
                eepromConfig.isFirstBootExceptID = true;    // Clear the flag
                saveEepromConfig();                         // Save to EEPROM if changed
                NVIC_SystemReset();                         // Perform software reset
            }
            // Address 502: Factory reset all data
            if (RTUServer.coilRead(MB_COIL_APPLY_FACTORY_RESET_ALL_DATA)) 
            {
                LOG_INFO_MODBUS(F("[MODBUS] Factory reset (all data) requested\n"));
                eepromConfig.isFirstBoot = true;        // Clear the flag
                saveEepromConfig();                     // Save to EEPROM if changed
                NVIC_SystemReset();                     // Perform software reset
            }
        }

        // Software reset (Addr.504)
        if (RTUServer.coilRead(MB_COIL_SOFTWARE_RESET)) 
        {
            LOG_INFO_MODBUS(F("[MODBUS] Software reset requested\n"));
            NVIC_SystemReset();         // Perform software reset
        }
        
        // Apply LED state changes (Addr.1001-1008)
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
                    LOG_DEBUG_LED("[LED] L" + String(i+1) + " turned ON\n");
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
                    LOG_DEBUG_LED("[LED] L" + String(i+1) + " turned OFF\n");
                }
                // printLedStatus();
            }
        }
    }
}

