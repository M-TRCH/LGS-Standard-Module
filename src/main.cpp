
#include <Arduino.h>
#include "config.h"
#include "system.h"
#include "led.h"
#include "modbus_utils.h"
#include "eeprom_utils.h"

uint8_t last_led_state[9] = {0};

void setup() 
{
    #ifdef SYSTEM_H
        sysInit();  // Initialize system
    #endif

    #ifdef EEPROM_UTILS_H
        // clearEeprom();
        // Serial.println("EEPROM cleared for testing purposes");
        // Serial.flush();
        // while (1);

        loadEepromConfig(); // Load configuration from EEPROM
        eepromConfig_cache = eepromConfig;

        if (eepromConfig.isFirstBoot == true) 
        {
            // write default values to EEPROM
            Serial.println("First time programming - writing default config to EEPROM");
            eepromConfig = eepromConfig_default;
            saveEepromConfig();                     // Save to EEPROM if changed
            delay(2000);
            NVIC_SystemReset();    
        }

        // eepromConfig.identifier = 18;           // Set a default identifier
        // eepromConfig.serialNumber = 1234;       // Set a default serial number
        // saveEepromConfig();                     // Save to EEPROM if changed
        
        Serial.println("EEPROM configuration loaded");
    #endif

    #ifdef LED_H
        ledInit();  // Initialize LEDs
    #endif

    #ifdef MODBUS_UTILS_H
        Serial.println("Modbus ID: " + String(eepromConfig.identifier));
        modbusInit(eepromConfig.identifier);  // Initialize Modbus

        RTUServer.holdingRegisterWrite(MB_REG_SERIAL_NUMBER, eepromConfig.serialNumber);
        RTUServer.holdingRegisterWrite(MB_REG_IDENTIFIER, eepromConfig.identifier);
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
                Serial.println("Factory Reset initiated via Modbus");
                eepromConfig.isFirstBoot = true;        // Clear the flag
                saveEepromConfig();                     // Save to EEPROM if changed
                digitalWrite(LED_RUN_PIN, LOW);
                delay(2000);
                NVIC_SystemReset();                     // Perform software reset
            }
            else
            {
                // Save current config to EEPROM
                Serial.println("Saving configuration to EEPROM via Modbus");
                saveEepromConfig();                     // Save to EEPROM if changed
                digitalWrite(LED_RUN_PIN, LOW);
                delay(2000);
                NVIC_SystemReset();                     // Perform software reset
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
