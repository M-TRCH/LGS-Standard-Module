
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
        // clearEeprom(true);  // Uncomment to clear EEPROM for debugging  

        loadEepromConfig(); // Load configuration from EEPROM
        
        if (eepromConfig.isFirstBoot == true) 
        {
            // write default values to EEPROM
            PRINT(DEBUG_BASIC, F("First time programming\n"));
            eepromConfig = eepromConfig_default;
            if (saveEepromConfig()) 
            {
                PRINT(DEBUG_BASIC, F("EEPROM saved\n"));
            } 
            else 
            {
                PRINT(DEBUG_BASIC, F("EEPROM changed\n"));
            }
            NVIC_SystemReset();    
        }
        PRINT(DEBUG_BASIC, F("EEPROM loaded\n"));
    #endif

    #ifdef LED_H
        ledInit();  // Initialize LEDs
    #endif

    #ifdef MODBUS_UTILS_H
        PRINT(DEBUG_BASIC, "Modbus ID: " + String(eepromConfig.identifier) + "\n");
        modbusInit(eepromConfig.identifier);  // Initialize Modbus
        eeprom2modbusMapping();
    #endif
}

void loop() 
{  
    // Poll Modbus server for requests
    if(RTUServer.poll()) 
    {   
        if (RTUServer.coilRead(MB_COIL_WRITE_TO_EEPROM)) 
        {
            if (RTUServer.coilRead(MB_COIL_FACTORY_RESET_ALL_DATA)) 
            {
                // Perform factory reset
                PRINT(DEBUG_BASIC, F("Factory Reset initiated via Modbus"));
                eepromConfig.isFirstBoot = true;        // Clear the flag
                saveEepromConfig();                     // Save to EEPROM if changed
                NVIC_SystemReset();                     // Perform software reset
            }
            else
            {
                // Save current config to EEPROM
                PRINT(DEBUG_BASIC, F("Saving configuration to EEPROM via Modbus"));
                modbus2eepromMapping();
                saveEepromConfig();         // Save to EEPROM if changed
                NVIC_SystemReset();         // Perform software reset
            }
        }


        // read the current value of the coil
        for (int i = 1; i <= 8; i++) 
        {
            int led_state = RTUServer.coilRead(1000 + i);
            if (led_state != last_led_state[i]) // Check if the LED state has changed
            {
                last_led_state[i] = led_state;

                if (led_state) // If the LED state is ON
                {
                    leds[i]->setPixelColor(0, leds[i]->Color(default_color[i][0], default_color[i][1], default_color[i][2]));
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
