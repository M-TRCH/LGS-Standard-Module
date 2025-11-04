
// Reference Documentation: .pdf

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
    modbusInit(eepromConfig.identifier);    // Initialize Modbus
    eeprom2modbusMapping();
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
    // LOG_DEBUG_SYS("Func SW: " + String(digitalRead(FUNC_SW_PIN)) + "\n");
    // LOG_DEBUG_SYS("[SYSTEM] functionMode: " + String(functionMode) + "\n");

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

