/**
 * @file main_example_func_switch.cpp
 * @brief Example showing how to use the function switch detection at startup
 * 
 * This example demonstrates how to check the function switch (FUNC_SW_PIN) 
 * during startup and perform different actions based on how long it was pressed.
 */

#include <Arduino.h>
#include "config.h"
#include "system.h"
#include "led.h"
#include "modbus_utils.h"
#include "eeprom_utils.h"

// Global variable to store the detected function switch mode
FunctionSwitchMode functionMode = FUNC_SW_NONE;

/**
 * @brief Handle SHORT press (>1 second)
 * Example: Enter diagnostic mode
 */
void handleShortPress() 
{
    LOG_INFO_SYS(F("[SYSTEM] Entering DIAGNOSTIC MODE\n"));
    
    // Example: Flash all LEDs in sequence
    for (int i = 0; i < LED_NUM; i++) 
    {
        leds[i]->setPixelColor(0, leds[i]->Color(255, 255, 255));
        leds[i]->show();
        delay(200);
        leds[i]->setPixelColor(0, leds[i]->Color(0, 0, 0));
        leds[i]->show();
    }
    
    LOG_INFO_SYS(F("[SYSTEM] Diagnostic mode complete, continuing normal operation\n"));
}

/**
 * @brief Handle MEDIUM press (>5 seconds)
 * Example: Reset to factory defaults (except Modbus ID)
 */
void handleMediumPress() 
{
    LOG_INFO_SYS(F("[SYSTEM] Entering FACTORY RESET MODE (Except ID)\n"));
    
    // Flash LEDs red to indicate factory reset
    for (int i = 0; i < LED_NUM; i++) 
    {
        leds[i]->setPixelColor(0, leds[i]->Color(255, 0, 0));
        leds[i]->show();
    }
    delay(1000);
    
    // Perform factory reset except ID
    eepromConfig.isFirstBootExceptID = true;
    saveEepromConfig();
    
    LOG_INFO_SYS(F("[SYSTEM] Factory reset (except ID) initiated, restarting...\n"));
    delay(500);
    
    // Reset the board
    NVIC_SystemReset();
}

/**
 * @brief Handle LONG press (>10 seconds)
 * Example: Complete factory reset (including Modbus ID)
 */
void handleLongPress() 
{
    LOG_INFO_SYS(F("[SYSTEM] Entering COMPLETE FACTORY RESET MODE\n"));
    
    // Flash all LEDs red rapidly to indicate complete reset
    for (int i = 0; i < 10; i++) 
    {
        for (int j = 0; j < LED_NUM; j++) 
        {
            leds[j]->setPixelColor(0, leds[j]->Color(255, 0, 0));
            leds[j]->show();
        }
        delay(100);
        for (int j = 0; j < LED_NUM; j++) 
        {
            leds[j]->setPixelColor(0, leds[j]->Color(0, 0, 0));
            leds[j]->show();
        }
        delay(100);
    }
    
    // Perform complete factory reset
    eepromConfig.isFirstBoot = true;
    saveEepromConfig();
    
    LOG_INFO_SYS(F("[SYSTEM] Complete factory reset initiated, restarting...\n"));
    delay(500);
    
    // Reset the board
    NVIC_SystemReset();
}

void setup() 
{
#ifdef SYSTEM_H
    sysInit(LOG_DEBUG);  // Initialize system
    
    // Check function switch immediately after system init
    functionMode = checkFunctionSwitch();
    
    // Handle the detected mode
    switch (functionMode) 
    {
        case FUNC_SW_SHORT:
            // Don't need to load EEPROM yet for diagnostic
            handleShortPress();
            break;
            
        case FUNC_SW_MEDIUM:
        case FUNC_SW_LONG:
            // Need to load EEPROM before reset operations
            #ifdef EEPROM_UTILS_H
                loadEepromConfig();
            #endif
            
            if (functionMode == FUNC_SW_MEDIUM) 
            {
                handleMediumPress();  // Will reset the board
            } 
            else 
            {
                handleLongPress();    // Will reset the board
            }
            break;
            
        case FUNC_SW_NONE:
        default:
            LOG_INFO_SYS(F("[SYSTEM] Normal operation mode\n"));
            break;
    }
#endif

#ifdef EEPROM_UTILS_H
    // Load EEPROM if not already loaded
    if (functionMode == FUNC_SW_NONE || functionMode == FUNC_SW_SHORT) 
    {
        handleFirstBoot();
        loadEepromConfig();
        LOG_INFO_EEPROM(F("[EEPROM] Configuration loaded\n"));
    }
#endif

#ifdef LED_H
    ledInit();  // Initialize LEDs
#endif

#ifdef MODBUS_UTILS_H
    LOG_INFO_MODBUS("[MODBUS] Initializing with ID: " + String(eepromConfig.identifier) + "\n");
    modbusInit(eepromConfig.identifier);
    eeprom2modbusMapping();
    LOG_INFO_MODBUS(F("[MODBUS] Initialization complete\n"));
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
        // Write data to EEPROM (Addr.503)
        if (RTUServer.coilRead(MB_COIL_WRITE_TO_EEPROM)) 
        {
            LOG_INFO_MODBUS(F("[MODBUS] Saving configuration to EEPROM\n"));
            modbus2eepromMapping();
            NVIC_SystemReset();
        }

        // Apply factory reset (Addr.500)
        if (RTUServer.coilRead(MB_COIL_FACTORY_RESET)) 
        {
            if (RTUServer.coilRead(MB_COIL_APPLY_FACTORY_RESET_EXCEPT_ID)) 
            {
                LOG_INFO_MODBUS(F("[MODBUS] Factory reset (except ID) requested\n"));
                eepromConfig.isFirstBootExceptID = true;
                saveEepromConfig();
                NVIC_SystemReset();
            }
            
            if (RTUServer.coilRead(MB_COIL_APPLY_FACTORY_RESET_ALL_DATA)) 
            {
                LOG_INFO_MODBUS(F("[MODBUS] Factory reset (all data) requested\n"));
                eepromConfig.isFirstBoot = true;
                saveEepromConfig();
                NVIC_SystemReset();
            }
        }

        // Software reset (Addr.504)
        if (RTUServer.coilRead(MB_COIL_SOFTWARE_RESET)) 
        {
            LOG_INFO_MODBUS(F("[MODBUS] Software reset requested\n"));
            NVIC_SystemReset();
        }

        // Apply LED state changes (Addr.1001-1008)
        for (int i = 0; i < LED_NUM; i++) 
        {
            int led_state = RTUServer.coilRead(MB_COIL_LED_1_ENABLE + i);
            if (led_state != last_led_state[i]) 
            {
                last_led_state[i] = led_state;

                if (led_state) 
                {
                    float brightness = RTUServer.holdingRegisterRead(MB_REG_LED_1_BRIGHTNESS + i*10) / 100.0;
                    leds[i]->setPixelColor(0, leds[i]->Color(
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_RED + i*10) * brightness,
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_GREEN + i*10) * brightness,
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_BLUE + i*10) * brightness));
                    leds[i]->show();

                    led_counter[i]++;
                    led_timer[i] = millis();
                    LOG_DEBUG_LED("[LED] L" + String(i+1) + " turned ON\n");
                }   
                else 
                {
                    leds[i]->setPixelColor(0, leds[i]->Color(0, 0, 0));
                    leds[i]->show();
                
                    if (led_timer[i] != 0) 
                    {
                        led_time_sum[i] += (millis() - led_timer[i]) / 1000.0;
                        led_timer[i] = 0;
                    }
                    LOG_DEBUG_LED("[LED] L" + String(i+1) + " turned OFF\n");
                }
            }
        }
    }

    // Update LED statistics and enforce max on-time limits
    for (int i = 0; i < LED_NUM; i++)
    {
        if (led_timer[i] != 0 && millis() - led_timer[i] > RTUServer.holdingRegisterRead(MB_REG_LED_1_MAX_ON_TIME + i*10) * 1000) 
        {
            LOG_WARNING_LED("[LED] L" + String(i+1) + " max on-time exceeded, turning off\n");
            leds[i]->setPixelColor(0, leds[i]->Color(0, 0, 0));
            leds[i]->show();
            last_led_state[i] = false;

            led_time_sum[i] += (millis() - led_timer[i]) / 1000.0;
            led_timer[i] = 0;

            RTUServer.coilWrite(MB_COIL_LED_1_ENABLE + i, 0);
        }

        RTUServer.holdingRegisterWrite(MB_REG_LED_1_ON_COUNTER + i*10, led_counter[i]);
        RTUServer.holdingRegisterWrite(MB_REG_LED_1_ON_TIME + i*10, (uint32_t)led_time_sum[i]);
    }
}
