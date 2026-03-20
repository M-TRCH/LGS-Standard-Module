
// Reference Documentation: LGS-Control-Table-Updated-271125.pdf

#include <Arduino.h>
#include "led.h"
#include "eeprom_utils.h"
#include "modbus_utils.h"

// for testing oled display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

void oled_init()
{
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    oled.setTextColor(WHITE);
    oled.setTextSize(2);    
    oled.setRotation(0);      
    oled.clearDisplay();    
    oled.display();
}

// ---- AT24C32D External EEPROM Test (I2C Address: 0x50) ----
#define AT24C32D_ADDR       0x50
#define AT24C32D_PAGE_SIZE  32      // AT24C32D page write size (bytes)
#define AT24C32D_TEST_ADDR  0x0000  // Start address for test data
#define AT24C32D_TEST_LEN   128     // Total bytes to write/read in test

// Write up to one page within the same page boundary
bool at24c32d_writePage(uint16_t memAddr, const uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(AT24C32D_ADDR);
    Wire.write((uint8_t)(memAddr >> 8));    // MSB of memory address
    Wire.write((uint8_t)(memAddr & 0xFF));  // LSB of memory address
    for (uint8_t i = 0; i < len; i++)
        Wire.write(buf[i]);
    uint8_t err = Wire.endTransmission();
    delay(5); // AT24C32D write cycle time (max 5ms)
    return (err == 0);
}

// Write multiple bytes using page writes (respects 32-byte page boundaries)
bool at24c32d_writeBytes(uint16_t memAddr, const uint8_t *buf, uint16_t len)
{
    uint16_t written = 0;
    while (written < len)
    {
        uint8_t pageOffset = (uint8_t)((memAddr + written) % AT24C32D_PAGE_SIZE);
        uint8_t pageRemain = AT24C32D_PAGE_SIZE - pageOffset;
        uint8_t toWrite    = ((len - written) < pageRemain) ? (uint8_t)(len - written) : pageRemain;

        if (!at24c32d_writePage(memAddr + written, buf + written, toWrite))
            return false;
        written += toWrite;
    }
    return true;
}

// Read multiple bytes sequentially (up to 32 bytes per requestFrom)
bool at24c32d_readBytes(uint16_t memAddr, uint8_t *buf, uint16_t len)
{
    Wire.beginTransmission(AT24C32D_ADDR);
    Wire.write((uint8_t)(memAddr >> 8));
    Wire.write((uint8_t)(memAddr & 0xFF));
    if (Wire.endTransmission() != 0) return false;

    uint16_t remaining = len;
    uint16_t offset = 0;
    while (remaining > 0)
    {
        uint8_t chunk = (remaining > 32) ? 32 : (uint8_t)remaining;
        Wire.requestFrom((uint8_t)AT24C32D_ADDR, chunk);
        uint8_t received = 0;
        while (Wire.available() && received < chunk)
        {
            buf[offset++] = Wire.read();
            received++;
        }
        if (received != chunk) return false;
        remaining -= chunk;
    }
    return true;
}

// testAT24C32D(doWrite):
//   doWrite = true  → write test pattern to EEPROM (run once before power cycle)
//   doWrite = false → read back and verify after power cycle
void testAT24C32D(bool doWrite = true)
{
    Serial.println(F("\n[AT24C32D] === External EEPROM Test ==="));
    Serial.println(doWrite ? F("[AT24C32D] Mode: WRITE") : F("[AT24C32D] Mode: READ (verify after power cycle)"));
    Serial.println("[AT24C32D] Address: 0x" + String(AT24C32D_TEST_ADDR, HEX) +
                   "  Length: " + String(AT24C32D_TEST_LEN) + " bytes");

    // Test pattern: sequential 0x00, 0x01, ..., 0xFF, 0x00, ...
    uint8_t testBuf[AT24C32D_TEST_LEN];
    for (uint16_t i = 0; i < AT24C32D_TEST_LEN; i++)
        testBuf[i] = (uint8_t)(i & 0xFF);

    if (doWrite)
    {
        // --- WRITE MODE ---
        Serial.println(F("[AT24C32D] Writing test pattern..."));
        Serial.print(F("[AT24C32D] Data: "));
        for (uint16_t i = 0; i < AT24C32D_TEST_LEN; i++)
        {
            if (testBuf[i] < 0x10) Serial.print("0");
            Serial.print(testBuf[i], HEX);
            Serial.print(" ");
            if ((i + 1) % 16 == 0) Serial.println();
        }

        if (at24c32d_writeBytes(AT24C32D_TEST_ADDR, testBuf, AT24C32D_TEST_LEN))
            Serial.println(F("[AT24C32D] Write OK. Power cycle the MCU, then run with doWrite=false to verify."));
        else
            Serial.println(F("[AT24C32D] Write FAILED - check I2C wiring and address 0x50"));
    }
    else
    {
        // --- READ & VERIFY MODE ---
        uint8_t readBuf[AT24C32D_TEST_LEN] = {0};
        Serial.println(F("[AT24C32D] Reading back data..."));

        if (!at24c32d_readBytes(AT24C32D_TEST_ADDR, readBuf, AT24C32D_TEST_LEN))
        {
            Serial.println(F("[AT24C32D] Read FAILED - check I2C wiring and address 0x50"));
            return;
        }

        Serial.print(F("[AT24C32D] Data: "));
        uint16_t errCount = 0;
        for (uint16_t i = 0; i < AT24C32D_TEST_LEN; i++)
        {
            if (readBuf[i] < 0x10) Serial.print("0");
            Serial.print(readBuf[i], HEX);
            Serial.print(" ");
            if ((i + 1) % 16 == 0) Serial.println();
            if (readBuf[i] != testBuf[i]) errCount++;
        }

        if (errCount == 0)
        {
            Serial.println(F("[AT24C32D] >> ALL PASS - Data retained correctly across power cycle!"));
        }
        else
        {
            Serial.println("[AT24C32D] >> FAIL - " + String(errCount) + " byte(s) mismatched");
            for (uint16_t i = 0; i < AT24C32D_TEST_LEN; i++)
            {
                if (readBuf[i] != testBuf[i])
                {
                    Serial.println("[AT24C32D]   First mismatch at addr 0x" + String(AT24C32D_TEST_ADDR + i, HEX) +
                                   ": expected 0x" + String(testBuf[i], HEX) +
                                   ", got 0x" + String(readBuf[i], HEX));
                    break;
                }
            }
        }
    }

    Serial.println(F("[AT24C32D] === Test Complete ===\n"));
}

void testLatch()
{
    while (true)
    {
        if (isLatchLocked())
            digitalWrite(LED_RUN_PIN, HIGH);
        else
            digitalWrite(LED_RUN_PIN, LOW);
        
        if (digitalRead(FUNC_SW_PIN) == LOW)
        {
            delay(50); // Debounce delay 
            if (digitalRead(FUNC_SW_PIN) == LOW)
            {   
                unlockLatch(500);
            }
        }
    }
}

void testControlPanel()
{
    static bool runLedState = false;
    modbusClientInit(); // Initialize Modbus RTU Client for broadcasting
   
    while (true)
    {
        if (digitalRead(FUNC_SW_PIN) == LOW)
        {
            delay(50); // Debounce delay 
            if (digitalRead(FUNC_SW_PIN) == LOW)
            {
                static uint32_t startFuncSW = millis();
                
                // Toggle RUN LED state
                runLedState = !runLedState;

                digitalWrite(LED_RUN_PIN, runLedState);
                broadcastAllLeds(runLedState); // Broadcast all LEDs with the same state

                while (digitalRead(FUNC_SW_PIN) == LOW)
                {
                    if (millis() - startFuncSW >= 3000) // timeout for long press
                    {
                        break;
                    }
                }
            }
        }  
    }
}

void setup() 
{
#ifdef SYSTEM_H
    sysInit(LOG_NONE);  // Initialize system

    // testLatch();        // Test latch functionality only
    // oled_init();        // Initialize OLED for testing
    testAT24C32D(false);  // true = WRITE mode, false = READ/verify mode (after power cycle)
#endif

#ifdef EEPROM_UTILS_H
    // clearEeprom(true);   // Uncomment to clear EEPROM for debugging  
    eepromInit();        // Initialize EEPROM and load configuration
#endif

#ifdef LED_H
    ledInit();  // Initialize LEDs
    while (1)   // Test LED8 strip
    {
        testLed8(255); 
    }
#endif

#ifdef MODBUS_UTILS_H
    // Initialize Modbus server with ID from EEPROM, default ID (247) or special ID (246) for SET_ID mode
    // modbusInit(functionMode == FUNC_SW_SET_ID ? DEFAULT_IDENTIFIER-1 : eepromConfig.identifier);    
    // eeprom2modbusMapping(); // Map EEPROM config to Modbus registers

    // testControlPanel(); // Test control panel functionality only
#endif
}

void loop() 
{   
#ifdef ENABLE_MAIN_LOOP
    /*
    // static uint32_t lastWriteDisp = 0;
    // static uint8_t numCnt = 0;
    // static uint8_t colorCnt = 0;
    // static uint8_t led_index = 0;
    // if (millis() - lastWriteDisp >= 1000) // Update display every second
    // {
    //     lastWriteDisp = millis();

    //     // Increment number counter
    //     numCnt++;
    //     if (numCnt > 100) numCnt = 0;
    //     // Increment color counter
    //     colorCnt++;
    //     if (colorCnt > 3) colorCnt = 0;

    //     oled.clearDisplay();
    //     oled.setCursor(18, 22);
    //     oled.println("LGS: " + String(numCnt));
    //     oled.display();

    //     for (int i = 0; i < 4; i++) 
    //     {
    //         if (i == 0) led_index = 2; 
    //         else if (i == 1) led_index = 3;
    //         else if (i == 2) led_index = 6;
    //         else if (i == 3) led_index = 7;

    //         if (colorCnt == 0)
    //             leds[led_index]->setPixelColor(0, leds[led_index]->Color(204,0,0)); // Red
    //         else if (colorCnt == 1)
    //             leds[led_index]->setPixelColor(0, leds[led_index]->Color(0,204,0)); // Green
    //         else if (colorCnt == 2)
    //             leds[led_index]->setPixelColor(0, leds[led_index]->Color(0,0,204)); // Blue
            
    //         // update LED
    //         leds[led_index]->show();
    //     }
    // }
    */

    // static uint32_t lastTestPrint = 0;
    // if (millis() - lastTestPrint >= 10) // Print every 1 seconds
    // {
    //     lastTestPrint = millis();
    //     LOG_DEBUG_SYS("[SYSTEM] Unlocked in " + String(RTUServer.holdingRegisterRead(MB_REG_TIME_AFTER_UNLOCK)) + " seconds\n");
    // }

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

        // Routine sensor read
        if (ON_ROUTINE_SENSOR_READ())
        {
            float temp;
            sts4x.measureHighPrecision(temp);
            RTUServer.holdingRegisterWrite(MB_REG_BUILT_IN_TEMP, (uint16_t)(temp * 100)); // Store temperature as integer (e.g., 2534 for 25.34°C)
            // LOG_DEBUG_SYS("Temperature: " + String(temp, 2) + " °C\n");
        }

        // Enforce max on-time limits for all LEDs
        for (int i = 0; i < LED_NUM; i++)
        {
            // Check if LED is ON and has exceeded max on-time limit
            if (led_timer[i] != 0 && RTUServer.holdingRegisterRead(MB_REG_LED_1_MAX_ON_TIME + i*10) > 0)
            {
                if (millis() - led_timer[i] > RTUServer.holdingRegisterRead(MB_REG_LED_1_MAX_ON_TIME + i*10) * 1000) // Convert seconds to ms
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
            }
        }

        // Update LED statistics to Modbus registers
        uint32_t total_led_counter = 0;
        uint32_t total_led_time = 0;
        for (int i = 0; i < LED_NUM; i++)
        {
            // Update individual LED statistics
            uint16_t counter_value = (led_counter[i] > 65535) ? 65535 : led_counter[i];
            uint16_t time_value = ((uint32_t)led_time_sum[i] > 65535) ? 65535 : (uint32_t)led_time_sum[i];
            
            RTUServer.holdingRegisterWrite(MB_REG_LED_1_ON_COUNTER + i*10, counter_value);     // Update on-count (clamped to uint16 max)
            RTUServer.holdingRegisterWrite(MB_REG_LED_1_ON_TIME + i*10, time_value);            // Update total on-time in seconds (clamped to uint16 max)
            
            // Accumulate totals
            total_led_counter += led_counter[i];
            total_led_time += (uint32_t)led_time_sum[i];
        }
        
        // Update total LED statistics (clamped to uint16 max)
        uint16_t total_counter_value = (total_led_counter > 65535) ? 65535 : total_led_counter;
        uint16_t total_time_value = (total_led_time > 65535) ? 65535 : total_led_time;
        
        RTUServer.holdingRegisterWrite(MB_REG_TOTAL_LED_ON_CNT, total_counter_value);      // Total LED on count (clamped to uint16 max)
        RTUServer.holdingRegisterWrite(MB_REG_TOTAL_LED_ON_TIME, total_time_value);        // Total LED on time in seconds (clamped to uint16 max)
    
        // Update time after last unlock
        if (isLatchLocked())  
        {
            lastTimeLatchLocked = millis();
            RTUServer.holdingRegisterWrite(MB_REG_TIME_AFTER_UNLOCK, 0); 
        }
        else
        {           
            RTUServer.holdingRegisterWrite(MB_REG_TIME_AFTER_UNLOCK, (uint16_t)((millis() - lastTimeLatchLocked) / 1000)); // in seconds
        }
    }

    // Poll Modbus server for requests
    if(RTUServer.poll()) 
    {   
        // Operation group:
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
        
        // Configuration group:
        // Global Brightness (Addr.190) - Set all LED brightness at once
        uint16_t global_brightness = RTUServer.holdingRegisterRead(MB_REG_GLOBAL_BRIGHTNESS);
        if (global_brightness != last_global_brightness && global_brightness >= 0 && global_brightness <= 100) 
        {
            last_global_brightness = global_brightness;
            for (int i = 0; i < LED_NUM; i++) 
            {
                RTUServer.holdingRegisterWrite(MB_REG_LED_1_BRIGHTNESS + i*10, global_brightness);
            }
            LOG_INFO_MODBUS("[MODBUS] Global brightness set to " + String(global_brightness) + "% for all LEDs\n");
        }

        // Global Max On Time (Addr.194) - Set all LED max on-time at once
        uint16_t global_max_on_time = RTUServer.holdingRegisterRead(MB_REG_GLOBAL_MAX_ON_TIME);
        if (global_max_on_time != last_global_max_on_time && global_max_on_time >= 0 && global_max_on_time <= 65535) 
        {
            last_global_max_on_time = global_max_on_time;
            for (int i = 0; i < LED_NUM; i++) 
            {
                RTUServer.holdingRegisterWrite(MB_REG_LED_1_MAX_ON_TIME + i*10, global_max_on_time);
            }
            LOG_INFO_MODBUS("[MODBUS] Global max on-time set to " + String(global_max_on_time) + " seconds for all LEDs\n");
        }

        // Control group:
        // Latch trigger (Addr.1020)
        if (RTUServer.coilRead(MB_COIL_LATCH_TRIGGER)) 
        {
            delay(RTUServer.holdingRegisterRead(MB_REG_UNLOCK_DELAY)); // Small delay to ensure coil state is stable
            unlockLatch(300);  // Unlock for 300ms (safety limit enforced in function)
            RTUServer.coilWrite(MB_COIL_LATCH_TRIGGER, 0); // Reset the coil
            LOG_INFO_MODBUS(F("[MODBUS] Latch unlock triggered via Modbus\n"));
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

        // Apply LED state changes with latch trigger (Addr.1021-1028)
        for (int i = 0; i < LED_NUM; i++) 
        {
            int led_latch_state = RTUServer.coilRead(MB_COIL_LED_1_LATCH + i);
            if (led_latch_state) // Check if the LED latch coil is triggered
            {
                // Turn ON the LED (same as LED enable)
                float brightness = RTUServer.holdingRegisterRead(MB_REG_LED_1_BRIGHTNESS + i*10) / 100.0;
                leds[i]->setPixelColor(0, leds[i]->Color(
                    RTUServer.holdingRegisterRead(MB_REG_LED_1_RED + i*10) * brightness,
                    RTUServer.holdingRegisterRead(MB_REG_LED_1_GREEN + i*10) * brightness,
                    RTUServer.holdingRegisterRead(MB_REG_LED_1_BLUE + i*10) * brightness));
                leds[i]->show();

                // Update LED state tracking
                if (!last_led_state[i]) 
                {
                    led_counter[i]++;
                    led_timer[i] = millis();
                    last_led_state[i] = true;
                    LOG_DEBUG_LED("[LED] L" + String(i+1) + " turned ON via latch\n");
                }

                // Trigger the latch unlock
                delay(RTUServer.holdingRegisterRead(MB_REG_UNLOCK_DELAY));
                unlockLatch(300);  // Unlock for 300ms (safety limit enforced in function)
                LOG_INFO_MODBUS("[MODBUS] Latch unlock triggered via LED" + String(i+1) + " latch coil\n");

                // Reset the latch coil and sync with enable coil
                RTUServer.coilWrite(MB_COIL_LED_1_LATCH + i, 0);
                RTUServer.coilWrite(MB_COIL_LED_1_ENABLE + i, 1);
            }
        }
    
    }
#endif
}

