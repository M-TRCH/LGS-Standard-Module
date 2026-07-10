#include <Arduino.h>
#include "app.h"
#include "system.h"
#include "eeprom_utils.h"
#include "hw/led.h"
#include "hw/sensor.h"
#include "hw/oled.h"
#include "hw/servo.h"
#include "modbus_utils.h"

// ---------------------------------------------------------------------------
// Internal helpers (module-private)
// ---------------------------------------------------------------------------

// Operating-mode handlers (selected by the function switch at startup)
static void runDemoMode();
static void runSetIdMode();
static void runFactoryResetMode();
static void runNormalMode();

// Modbus request processing
static void handleModbusRequests();

// Shared runtime tasks
static void applyLedColor(int ledIndex, float brightnessScale);
static void enforceLedMaxOnTime();
static void updateLedStatistics();
static void updateLatchStatus();
static void updateOledStatus(const String &title, const String &value = "");
static void updateOledCounter(uint8_t value);

static bool oledReady = false;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void appInit()
{
    // Initialize system core (pins, serial, sensor, function switch check)
    sysInit(LOG_NONE);

    // clearEeprom(true);   // Uncomment to clear EEPROM for debugging
    eepromInit();           // Initialize EEPROM and load configuration

    ledInit();              // Initialize LED ring
    oledReady = oledInit(); // Initialize OLED display (I2C2)
    servoInit();            // Initialize servo outputs (PC6, PC7)

    // Initialize Modbus server with ID from EEPROM, or special ID (246) for SET_ID mode
    modbusInit(functionMode == FUNC_SW_SET_ID ? DEFAULT_IDENTIFIER - 1 : eepromConfig.identifier);
    eeprom2modbusMapping(); // Map EEPROM config to Modbus registers
}

void appRun()
{
    // Dispatch based on the mode selected via the function switch at startup
    switch (functionMode)
    {
        case FUNC_SW_DEMO:          runDemoMode();          break;
        case FUNC_SW_SET_ID:        runSetIdMode();         break;
        case FUNC_SW_FACTORY_RESET: runFactoryResetMode();  break;
        case FUNC_SW_RUN:           runNormalMode();        break;
        default:                                            break;
    }

    // Service pending Modbus requests
    if (RTUServer.poll())
    {
        handleModbusRequests();
    }
}

// ---------------------------------------------------------------------------
// Operating-mode handlers
// ---------------------------------------------------------------------------

// Demo mode: cycle/blink all LEDs using their configured colors
static void runDemoMode()
{
    static uint32_t lastDemoFrame = 0;
    static uint16_t rainbowPhase = 0;
    static uint8_t oledCounter = 0;
    static bool lastSwitchPressed = false;
    static uint32_t lastSwitchEdge = 0;

    bool switchPressed = sysIsFunctionSwitchPressed();
    if (switchPressed && !lastSwitchPressed && (millis() - lastSwitchEdge >= 180))
    {
        lastSwitchEdge = millis();
        oledCounter = (oledCounter + 1) % 100;
    }
    lastSwitchPressed = switchPressed;

    if (millis() - lastDemoFrame >= 32)
    {
        lastDemoFrame = millis();
        rainbowPhase += 1;

        updateOledCounter(oledCounter);
        ledShowRainbowRipple(rainbowPhase);
    }
}

// Set ID mode: blink all LEDs in blue for identification
static void runSetIdMode()
{
    if (ON_ROUTINE_BLINK_SET_ID())
    {
        for (int i = 0; i < LED_NUM; i++)
        {
            ledSetAllPixels(i, ledColor(0, 0, (blink_set_id_state ? 204 : 0))); // Blue or off (magic number)
        }
    }
}

// Factory reset mode: solid red on all LEDs for 5 seconds, then reset
static void runFactoryResetMode()
{
    for (int i = 0; i < LED_NUM; i++)
    {
        ledSetAllPixels(i, ledColor(204, 0, 0)); // Red (magic number)
    }
    delay(5000); // (magic number)
    LOG_INFO_SYS(F("[SYSTEM] Factory reset mode engaged.\n"));
    eepromConfig.isFirstBoot = true;    // Set the flag
    saveEepromConfig();                 // Save to EEPROM if changed
    NVIC_SystemReset();                 // Perform software reset
}

// Normal operation: run LED heartbeat, sensor read, on-time limits and statistics
static void runNormalMode()
{
    // Routine blink for run LED
    if (ON_ROUTINE_BLINK_RUN())
    {
        sysSetRunIndicator(blink_run_state);
    }

    // Routine sensor read
    if (ON_ROUTINE_SENSOR_READ())
    {
        float temperatureC = 0.0f;
        if (sensorReadTemperature(temperatureC))
        {
            RTUServer.holdingRegisterWrite(MB_REG_BUILT_IN_TEMP, (uint16_t)(temperatureC * 100));
        }
    }

    enforceLedMaxOnTime();  // Turn off LEDs that exceeded their max on-time
    updateLedStatistics();  // Publish LED counters/timers to Modbus registers
    updateLatchStatus();    // Publish time-after-unlock to Modbus registers
}

// ---------------------------------------------------------------------------
// Runtime tasks
// ---------------------------------------------------------------------------

// Apply an LED strip's configured RGB color (from Modbus registers), scaled by
// its configured brightness and an additional runtime scale factor.
static void applyLedColor(int ledIndex, float brightnessScale)
{
    // i*10 to jump to next LED's registers (E.g., 110, 120, 130...)
    float brightness = (RTUServer.holdingRegisterRead(MB_REG_LED_1_BRIGHTNESS + ledIndex * 10) / 100.0f) * brightnessScale;
    ledSetAllPixels(ledIndex, ledColor(
        RTUServer.holdingRegisterRead(MB_REG_LED_1_RED + ledIndex * 10) * brightness,
        RTUServer.holdingRegisterRead(MB_REG_LED_1_GREEN + ledIndex * 10) * brightness,
        RTUServer.holdingRegisterRead(MB_REG_LED_1_BLUE + ledIndex * 10) * brightness));
}

// Enforce the per-LED maximum on-time limits.
static void enforceLedMaxOnTime()
{
    for (int i = 0; i < LED_NUM; i++)
    {
        // Check if LED is ON and has exceeded max on-time limit
        if (led_timer[i] != 0 && RTUServer.holdingRegisterRead(MB_REG_LED_1_MAX_ON_TIME + i * 10) > 0)
        {
            if (millis() - led_timer[i] > RTUServer.holdingRegisterRead(MB_REG_LED_1_MAX_ON_TIME + i * 10) * 1000) // Convert seconds to ms
            {
                LOG_WARNING_LED("[LED] L" + String(i + 1) + " max on-time exceeded, turning off\n");

                // Turn off the LED
                ledSetAllPixels(i, ledColor(0, 0, 0));
                last_led_state[i] = false; // Update last known state

                // Stop timer and accumulate time
                led_time_sum[i] += (millis() - led_timer[i]) / 1000.0; // Convert ms to seconds
                led_timer[i] = 0; // Reset timer

                // Update Modbus coil to reflect LED is off
                RTUServer.coilWrite(MB_COIL_LED_1_ENABLE + i, 0);
            }
        }
    }
}

// Publish per-LED and total LED statistics to Modbus registers.
static void updateLedStatistics()
{
    uint32_t total_led_counter = 0;
    uint32_t total_led_time = 0;
    for (int i = 0; i < LED_NUM; i++)
    {
        // Update individual LED statistics (clamped to uint16 max)
        uint16_t counter_value = (led_counter[i] > 65535) ? 65535 : led_counter[i];
        uint16_t time_value = ((uint32_t)led_time_sum[i] > 65535) ? 65535 : (uint32_t)led_time_sum[i];

        RTUServer.holdingRegisterWrite(MB_REG_LED_1_ON_COUNTER + i * 10, counter_value); // On-count
        RTUServer.holdingRegisterWrite(MB_REG_LED_1_ON_TIME + i * 10, time_value);        // Total on-time in seconds

        // Accumulate totals
        total_led_counter += led_counter[i];
        total_led_time += (uint32_t)led_time_sum[i];
    }

    // Update total LED statistics (clamped to uint16 max)
    uint16_t total_counter_value = (total_led_counter > 65535) ? 65535 : total_led_counter;
    uint16_t total_time_value = (total_led_time > 65535) ? 65535 : total_led_time;

    RTUServer.holdingRegisterWrite(MB_REG_TOTAL_LED_ON_CNT, total_counter_value);   // Total LED on count
    RTUServer.holdingRegisterWrite(MB_REG_TOTAL_LED_ON_TIME, total_time_value);     // Total LED on time in seconds
}

// Update the "time after unlock" register based on latch state.
static void updateLatchStatus()
{
#ifndef DISABLE_LATCH_STATUS_RESET
    if (isLatchLocked())
    {
        lastTimeLatchLocked = millis();
        RTUServer.holdingRegisterWrite(MB_REG_TIME_AFTER_UNLOCK, 0);
    }
    else
    {
        RTUServer.holdingRegisterWrite(MB_REG_TIME_AFTER_UNLOCK, (uint16_t)((millis() - lastTimeLatchLocked) / 1000)); // in seconds
    }
#else
    RTUServer.holdingRegisterWrite(MB_REG_TIME_AFTER_UNLOCK, 0); // Always report 0 since latch status reset is disabled
#endif
}

static void updateOledStatus(const String &title, const String &value)
{
    if (!oledReady)
    {
        return;
    }

    String screenText = title;
    if (value.length() > 0)
    {
        screenText += "\n" + value;
    }

    oledPrint(screenText, 2);
}

static void updateOledCounter(uint8_t value)
{
    if (!oledReady)
    {
        return;
    }

    oledPrintLargeNumber(value);
}

// ---------------------------------------------------------------------------
// Modbus request processing
// ---------------------------------------------------------------------------

static void handleModbusRequests()
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
            eepromConfig.isFirstBootExceptID = true;    // Set the flag
            saveEepromConfig();                         // Save to EEPROM if changed
            NVIC_SystemReset();                         // Perform software reset
        }
        // Address 502: Factory reset all data
        if (RTUServer.coilRead(MB_COIL_APPLY_FACTORY_RESET_ALL_DATA))
        {
            LOG_INFO_MODBUS(F("[MODBUS] Factory reset (all data) requested\n"));
            eepromConfig.isFirstBoot = true;        // Set the flag
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
            RTUServer.holdingRegisterWrite(MB_REG_LED_1_BRIGHTNESS + i * 10, global_brightness);
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
            RTUServer.holdingRegisterWrite(MB_REG_LED_1_MAX_ON_TIME + i * 10, global_max_on_time);
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
                applyLedColor(i, 1.0f);

                led_counter[i]++;           // Increment counter for how many times this LED has been turned on
                led_timer[i] = millis();    // Start timer for how long this LED has been on
                LOG_DEBUG_LED("[LED] L" + String(i + 1) + " turned ON\n");
            }
            else    // If the LED state is OFF
            {
                ledSetAllPixels(i, ledColor(0, 0, 0));

                // Stop timer and accumulate time if it was running
                if (led_timer[i] != 0)
                {
                    led_time_sum[i] += (millis() - led_timer[i]) / 1000.0; // Convert ms to seconds
                    led_timer[i] = 0; // Reset timer
                }
                LOG_DEBUG_LED("[LED] L" + String(i + 1) + " turned OFF\n");
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
            applyLedColor(i, 1.0f);

            // Update LED state tracking
            if (!last_led_state[i])
            {
                led_counter[i]++;
                led_timer[i] = millis();
                last_led_state[i] = true;
                LOG_DEBUG_LED("[LED] L" + String(i + 1) + " turned ON via latch\n");
            }

            // Trigger the latch unlock
            delay(RTUServer.holdingRegisterRead(MB_REG_UNLOCK_DELAY));
            unlockLatch(300);  // Unlock for 300ms (safety limit enforced in function)
            LOG_INFO_MODBUS("[MODBUS] Latch unlock triggered via LED" + String(i + 1) + " latch coil\n");

            // Reset the latch coil and sync with enable coil
            RTUServer.coilWrite(MB_COIL_LED_1_LATCH + i, 0);
            RTUServer.coilWrite(MB_COIL_LED_1_ENABLE + i, 1);
        }
    }
}
