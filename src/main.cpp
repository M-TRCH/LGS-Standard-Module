
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
                    float brightness = RTUServer.holdingRegisterRead(MB_REG_LED_1_BRIGHTNESS + i*10) / 100.0;
                    leds[i]->setPixelColor(0, leds[i]->Color(
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_RED + i*10) * brightness,
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_GREEN + i*10) * brightness,
                        RTUServer.holdingRegisterRead(MB_REG_LED_1_BLUE + i*10) * brightness));
                    leds[i]->show();
                }
                else
                {
                    leds[i]->setPixelColor(0, leds[i]->Color(0, 0, 0));
                    leds[i]->show();
                }
            }
        }
    }
}
