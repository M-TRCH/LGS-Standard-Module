#include "app.h"
#include "system.h"
#include "led.h"
#include "modbus_utils.h"
#include "eeprom_utils.h"
#include "hal/hal_latch.h"
#include "hal/hal_sensor.h"
#include "hal/hal_display.h"

// ---------------------------------------------------------------------------
// Named constants (replacing former magic numbers)
// ---------------------------------------------------------------------------
static constexpr uint8_t  STATUS_COLOR_LEVEL   = 204;   // ~80% of 255 for status colors
static constexpr uint16_t FACTORY_RESET_HOLD_MS = 5000; // Red hold time before reset
static constexpr uint16_t LATCH_PULSE_MS        = 300;  // Latch unlock pulse (clamped in HAL)
static constexpr uint32_t STAT_UINT16_MAX       = 65535;// Saturation limit for 16-bit stats

// ---------------------------------------------------------------------------
// Channel (LED) helpers - single source of truth, no duplicated color logic
// ---------------------------------------------------------------------------

// Read a channel's configured color from Modbus registers and drive its pixels.
// brightnessScale (0.0-1.0) allows blink dimming without touching the registers.
static void applyChannelColor(uint8_t i, float brightnessScale = 1.0f)
{
    float brightness = (RTUServer.holdingRegisterRead(mbLedCfg(i, LED_CFG_BRIGHTNESS)) / 100.0f) * brightnessScale;
    ledSetAllPixels(i, leds[i]->Color(
        RTUServer.holdingRegisterRead(mbLedCfg(i, LED_CFG_RED))   * brightness,
        RTUServer.holdingRegisterRead(mbLedCfg(i, LED_CFG_GREEN)) * brightness,
        RTUServer.holdingRegisterRead(mbLedCfg(i, LED_CFG_BLUE))  * brightness));
}

// Transition a channel OFF->ON: apply color and update on-statistics/timers.
static void turnChannelOn(uint8_t i)
{
    applyChannelColor(i);
    led_counter[i]++;
    led_timer[i] = millis();
    last_led_state[i] = true;
}

// Transition a channel ON->OFF: clear pixels and accumulate runtime.
static void turnChannelOff(uint8_t i)
{
    ledSetAllPixels(i, leds[i]->Color(0, 0, 0));
    if (led_timer[i] != 0)
    {
        led_time_sum[i] += (millis() - led_timer[i]) / 1000.0f; // ms -> seconds
        led_timer[i] = 0;
    }
    last_led_state[i] = false;
}

// ---------------------------------------------------------------------------
// Operating modes
// ---------------------------------------------------------------------------

// Demo: cycle every channel's color, blinking in sync with blink_demo_state.
static void runDemoMode()
{
    if (!ON_ROUTINE_BLINK_DEMO()) return;
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        applyChannelColor(i, blink_demo_state ? 1.0f : 0.0f);
    }
}

// Set-ID: blink all channels blue for physical identification.
static void runSetIdMode()
{
    if (!ON_ROUTINE_BLINK_SET_ID()) return;
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        ledSetAllPixels(i, leds[i]->Color(0, 0, blink_set_id_state ? STATUS_COLOR_LEVEL : 0));
    }
}

// Factory reset: solid red on every channel, then reset the board.
static void runFactoryResetMode()
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        ledSetAllPixels(i, leds[i]->Color(STATUS_COLOR_LEVEL, 0, 0));
    }
    delay(FACTORY_RESET_HOLD_MS);
    LOG_INFO_SYS(F("[SYSTEM] Factory reset mode engaged.\n"));
    eepromConfig.isFirstBoot = true;
    saveEepromConfig();
    NVIC_SystemReset();
}

// Update the optional OLED with the configured number when the display is on.
static void updateDisplay()
{
    static uint16_t lastDisplayNum = 0xFFFF;
    static bool lastDisplayEnabled = false;

    bool enabled = RTUServer.coilRead(MB_COIL_DISPLAY_ENABLE);
    if (enabled != lastDisplayEnabled)
    {
        displayEnable(enabled);
        lastDisplayEnabled = enabled;
        lastDisplayNum = 0xFFFF; // force refresh
    }

    if (enabled)
    {
        uint16_t num = RTUServer.holdingRegisterRead(MB_REG_SET_NUM_DISPLAY);
        if (num != lastDisplayNum)
        {
            displayShowNumber(num);
            lastDisplayNum = num;
        }
    }
}

// Normal operation: status LED, sensor, max-on-time enforcement, statistics.
static void runNormalMode()
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
        if (sensorReadTemperature(temp))
        {
            RTUServer.holdingRegisterWrite(MB_REG_BUILT_IN_TEMP, (uint16_t)(temp * 100)); // e.g. 2534 = 25.34 C
        }
    }

    // Enforce max on-time limits for all channels
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        uint16_t maxOnTime = RTUServer.holdingRegisterRead(mbLedCfg(i, LED_CFG_MAX_ON_TIME));
        if (led_timer[i] != 0 && maxOnTime > 0)
        {
            if (millis() - led_timer[i] > (uint32_t)maxOnTime * 1000) // seconds -> ms
            {
                LOG_WARNING_LED("[LED] L" + String(i + 1) + " max on-time exceeded, turning off\n");
                turnChannelOff(i);
                RTUServer.coilWrite(mbLedEnableCoil(i), 0);
            }
        }
    }

    // Update per-channel and total statistics (saturated to 16-bit)
    uint32_t total_led_counter = 0;
    uint32_t total_led_time = 0;
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        uint16_t counter_value = (led_counter[i] > STAT_UINT16_MAX) ? STAT_UINT16_MAX : led_counter[i];
        uint16_t time_value = ((uint32_t)led_time_sum[i] > STAT_UINT16_MAX) ? STAT_UINT16_MAX : (uint32_t)led_time_sum[i];

        RTUServer.holdingRegisterWrite(mbLedStatus(i, LED_STAT_ON_COUNTER), counter_value);
        RTUServer.holdingRegisterWrite(mbLedStatus(i, LED_STAT_ON_TIME), time_value);

        total_led_counter += led_counter[i];
        total_led_time += (uint32_t)led_time_sum[i];
    }

    RTUServer.holdingRegisterWrite(MB_REG_TOTAL_LED_ON_CNT,
        (total_led_counter > STAT_UINT16_MAX) ? STAT_UINT16_MAX : total_led_counter);
    RTUServer.holdingRegisterWrite(MB_REG_TOTAL_LED_ON_TIME,
        (total_led_time > STAT_UINT16_MAX) ? STAT_UINT16_MAX : total_led_time);

    // Update time since last unlock
#ifndef DISABLE_LATCH_STATUS_RESET
    if (isLatchLocked())
    {
        lastTimeLatchLocked = millis();
        RTUServer.holdingRegisterWrite(MB_REG_TIME_AFTER_UNLOCK, 0);
    }
    else
    {
        RTUServer.holdingRegisterWrite(MB_REG_TIME_AFTER_UNLOCK, (uint16_t)((millis() - lastTimeLatchLocked) / 1000));
    }
#else
    RTUServer.holdingRegisterWrite(MB_REG_TIME_AFTER_UNLOCK, 0);
#endif

    // Optional display output
    updateDisplay();
}

// ---------------------------------------------------------------------------
// Modbus request handling (executed after a successful poll)
// ---------------------------------------------------------------------------

static void handleOperationCoils()
{
    // Write configuration to EEPROM (Addr.503)
    if (RTUServer.coilRead(MB_COIL_WRITE_TO_EEPROM))
    {
        LOG_INFO_MODBUS(F("[MODBUS] Saving configuration to EEPROM\n"));
        modbus2eepromMapping();
        NVIC_SystemReset();
    }

    // Factory reset (Addr.500 + 501/502)
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
}

static void handleGlobalConfig()
{
    // Global Brightness (Addr.190) - apply to all channels
    uint16_t global_brightness = RTUServer.holdingRegisterRead(MB_REG_GLOBAL_BRIGHTNESS);
    if (global_brightness != last_global_brightness && global_brightness <= 100)
    {
        last_global_brightness = global_brightness;
        for (uint8_t i = 0; i < LED_NUM; i++)
        {
            RTUServer.holdingRegisterWrite(mbLedCfg(i, LED_CFG_BRIGHTNESS), global_brightness);
        }
        LOG_INFO_MODBUS("[MODBUS] Global brightness set to " + String(global_brightness) + "% for all LEDs\n");
    }

    // Global Max On Time (Addr.194) - apply to all channels
    uint16_t global_max_on_time = RTUServer.holdingRegisterRead(MB_REG_GLOBAL_MAX_ON_TIME);
    if (global_max_on_time != last_global_max_on_time)
    {
        last_global_max_on_time = global_max_on_time;
        for (uint8_t i = 0; i < LED_NUM; i++)
        {
            RTUServer.holdingRegisterWrite(mbLedCfg(i, LED_CFG_MAX_ON_TIME), global_max_on_time);
        }
        LOG_INFO_MODBUS("[MODBUS] Global max on-time set to " + String(global_max_on_time) + " seconds for all LEDs\n");
    }
}

static void handleLatchTrigger()
{
    if (RTUServer.coilRead(MB_COIL_LATCH_TRIGGER))
    {
        delay(RTUServer.holdingRegisterRead(MB_REG_UNLOCK_DELAY));
        unlockLatch(LATCH_PULSE_MS);
        RTUServer.coilWrite(MB_COIL_LATCH_TRIGGER, 0);
        LOG_INFO_MODBUS(F("[MODBUS] Latch unlock triggered via Modbus\n"));
    }
}

static void handleChannelEnable()
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        int led_state = RTUServer.coilRead(mbLedEnableCoil(i));
        if (led_state != last_led_state[i])
        {
            if (led_state)
            {
                turnChannelOn(i);
                LOG_DEBUG_LED("[LED] L" + String(i + 1) + " turned ON\n");
            }
            else
            {
                turnChannelOff(i);
                LOG_DEBUG_LED("[LED] L" + String(i + 1) + " turned OFF\n");
            }
        }
    }
}

static void handleChannelLatch()
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        if (!RTUServer.coilRead(mbLedLatchCoil(i))) continue;

        // Turn the channel ON (same as enable)
        applyChannelColor(i);
        if (!last_led_state[i])
        {
            led_counter[i]++;
            led_timer[i] = millis();
            last_led_state[i] = true;
            LOG_DEBUG_LED("[LED] L" + String(i + 1) + " turned ON via latch\n");
        }

        // Trigger the latch unlock
        delay(RTUServer.holdingRegisterRead(MB_REG_UNLOCK_DELAY));
        unlockLatch(LATCH_PULSE_MS);
        LOG_INFO_MODBUS("[MODBUS] Latch unlock triggered via LED" + String(i + 1) + " latch coil\n");

        // Reset the latch coil and sync with the enable coil
        RTUServer.coilWrite(mbLedLatchCoil(i), 0);
        RTUServer.coilWrite(mbLedEnableCoil(i), 1);
    }
}

static void processModbusRequests()
{
    handleOperationCoils();
    handleGlobalConfig();
    handleLatchTrigger();
    handleChannelEnable();
    handleChannelLatch();
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
void appRun()
{
    switch (functionMode)
    {
        case FUNC_SW_DEMO:          runDemoMode();         break;
        case FUNC_SW_SET_ID:        runSetIdMode();        break;
        case FUNC_SW_FACTORY_RESET: runFactoryResetMode(); break; // resets the board
        case FUNC_SW_RUN:
        default:                    runNormalMode();       break;
    }

    if (RTUServer.poll())
    {
        processModbusRequests();
    }
}
