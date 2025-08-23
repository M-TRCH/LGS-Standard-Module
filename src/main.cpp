
#include <Arduino.h>
#include "system.h"
#include "led.h"

#define MODBUS_SERIAL       0x00
#define MODBUS_SERIAL3      0x01
#define MODBUS_ID           18                  // Slave Identifier
#define MODBUS_SELECT       MODBUS_SERIAL       // Select the physical output

uint8_t last_led_state[8] = {0};

void setup() 
{
    #ifdef SYSTEM_H
        sysInit();  // Initialize system
    #endif

    #ifdef LED_H
        ledInit();  // Initialize LEDs
    #endif

    #if MODBUS_SELECT == MODBUS_SERIAL
        ModbusRTUServer.begin(rs485, MODBUS_ID, MODBUS_BAUD, SERIAL_8N1);   // Start Modbus RTU server with ID 1, baud rate 9600, and config SERIAL_8N1
    #elif MODBUS_SELECT == MODBUS_SERIAL3
        ModbusRTUServer.begin(rs4853, MODBUS_ID, MODBUS_BAUD, SERIAL_8N1);  // Start Modbus RTU server with Serial3
    #endif

    ModbusRTUServer.configureCoils(0x00, 8);
    ModbusRTUServer.configureDiscreteInputs(0x00, 8);
    ModbusRTUServer.configureHoldingRegisters(0x00, 8);
    ModbusRTUServer.configureInputRegisters(0x00, 8);
}

void loop() 
{
    if(ModbusRTUServer.poll()) 
    {
        // read the current value of the coil
        for (int i = 0; i < 8; i++) 
        {
            int led_state = ModbusRTUServer.coilRead(0x00 + i);
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
