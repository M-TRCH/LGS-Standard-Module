
#include <Arduino.h>
#include "system.h"

#define MODBUS_SERIAL       0x00
#define MODBUS_SERIAL3      0x01
#define MODBUS_ID           18                  // Slave Identifier
#define MODBUS_BAUD         9600
#define MODBUS_SELECT       MODBUS_SERIAL       // Select the physical output

#include <Adafruit_NeoPixel.h>
#define LED_NUM   1
#define LED_POWER 0.5f
Adafruit_NeoPixel led1(LED_NUM, PB1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led2(LED_NUM, PB2,  NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led3(LED_NUM, PA11, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led4(LED_NUM, PA8, NEO_GRB + NEO_KHZ800); 
Adafruit_NeoPixel led5(LED_NUM, PB0, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led6(LED_NUM, PC13,  NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led7(LED_NUM, PB14, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led8(LED_NUM, PB15, NEO_GRB + NEO_KHZ800); 
uint8_t last_led_state[8] = {0};

void setup() 
{
    sysInit();

    #if MODBUS_SELECT == MODBUS_SERIAL
        ModbusRTUServer.begin(rs485, MODBUS_ID, MODBUS_BAUD, SERIAL_8N1); // Start Modbus RTU server with ID 1, baud rate 9600, and config SERIAL_8N1
    #elif MODBUS_SELECT == MODBUS_SERIAL3
        ModbusRTUServer.begin(rs4853, MODBUS_ID, MODBUS_BAUD, SERIAL_8N1); // Start Modbus RTU server with Serial3
    #endif

    ModbusRTUServer.configureCoils(0x00, 8);

    // Initialize NeoPixel LEDs
    led1.begin();
    led2.begin();
    led3.begin();
    led4.begin();
    led5.begin();
    led6.begin();
    led7.begin();
    led8.begin();
}

void loop() 
{
    // digitalWrite(RUN_LED_PIN, HIGH);  // Turn the LED on
    // delay(200);                  // Wait for a second
    // digitalWrite(RUN_LED_PIN, LOW);   // Turn the LED off
    // delay(200);                  // Wait for a second
    // Serial.println("Hello, World!");  // Print a message to the serial monitor

    // poll for Modbus RTU requests
    int packetReceived = ModbusRTUServer.poll();

    if(packetReceived) 
    {
        // read the current value of the coil
        for (int i = 0; i < 8; i++) 
        {
            int led_state = ModbusRTUServer.coilRead(0x00 + i);
            if (led_state != last_led_state[i]) 
            {
                last_led_state[i] = led_state;

                if (led_state) 
                {
                    switch (i) 
                    {
                        case 0: // แดง
                            led1.setPixelColor(0, led1.Color(255 * LED_POWER, 0, 0));
                            led1.show();
                            break;
                        case 1: // เขียว
                            led2.setPixelColor(0, led2.Color(0, 255 * LED_POWER, 0));
                            led2.show();
                            break;
                        case 2: // น้ำเงิน
                            led3.setPixelColor(0, led3.Color(0, 0, 255 * LED_POWER));
                            led3.show();
                            break;
                        case 3: // เหลือง
                            led4.setPixelColor(0, led4.Color(255 * LED_POWER, 255 * LED_POWER, 0));
                            led4.show();
                            break;
                        case 4: // ฟ้า
                            led5.setPixelColor(0, led5.Color(0, 255 * LED_POWER, 255 * LED_POWER));
                            led5.show();
                            break;
                        case 5: // ม่วง
                            led6.setPixelColor(0, led6.Color(255 * LED_POWER, 0, 255 * LED_POWER));
                            led6.show();
                            break;
                        case 6: // ส้ม
                            led7.setPixelColor(0, led7.Color(255 * LED_POWER, 128 * LED_POWER, 0));
                            led7.show();
                            break;
                        case 7: // ขาว
                            led8.setPixelColor(0, led8.Color(255 * LED_POWER, 255 * LED_POWER, 255 * LED_POWER));
                            led8.show();
                            break;
                    }
                } 
                else
                {
                    switch (i)
                    {
                        case 0:
                            led1.setPixelColor(0, led1.Color(0, 0, 0));
                            led1.show();
                            break;
                        case 1:
                            led2.setPixelColor(0, led2.Color(0, 0, 0));
                            led2.show();
                            break;
                        case 2:
                            led3.setPixelColor(0, led3.Color(0, 0, 0));
                            led3.show();
                            break;
                        case 3:
                            led4.setPixelColor(0, led4.Color(0, 0, 0));
                            led4.show();
                            break;
                        case 4:
                            led5.setPixelColor(0, led5.Color(0, 0, 0));
                            led5.show();
                            break;
                        case 5:
                            led6.setPixelColor(0, led6.Color(0, 0, 0));
                            led6.show();
                            break;
                        case 6:
                            led7.setPixelColor(0, led7.Color(0, 0, 0));
                            led7.show();
                            break;
                        case 7:
                            led8.setPixelColor(0, led8.Color(0, 0, 0));
                            led8.show();
                            break;
                        
                        default:
                            break;
                    }
                    
                }
            }
        }

    }
}
