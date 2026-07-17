#include <Arduino.h>
#include <IWatchdog.h>
#include "app.h"
#include "config.h"
#include "util/periodic_timer.h"
#include "app/modes.h"
#include "app/latch_control.h"
#include "app/led_control.h"
#include "app/ops.h"
#include "app/display_control.h"
#include "app/servo_control.h"
#include "app/ota_control.h"
#include "app/diag_control.h"
#include "drivers/board_io.h"
#include "drivers/rs485_port.h"
#include "drivers/led_ring.h"
#include "drivers/temp_sensor.h"
#include "drivers/oled.h"
#include "svc/settings.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

// ---------------------------------------------------------------------------
// Internal helpers (module-private)
// ---------------------------------------------------------------------------

// Operating-mode handlers (selected by the function switch at startup)
static void runDemoMode();
static void runSetIdMode();
static void runFactoryResetMode();
static void runNormalMode();

static void updateOledCounter(uint8_t value);
static void updateOledSetId(uint16_t id);
static void updateOledFactoryCountdown(uint16_t secs);

static bool oledReady = false;
static FunctionSwitchMode functionMode = FUNC_SW_RUN; // selected once at boot

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void appInit()
{
    boardIoInit();          // discrete pins first (drives latch MOSFET LOW = safety)

    // Initialize the LED ring EARLY: ledRing.begin() drives the WS2812B data
    // pin (PA8) LOW and blanks the ring within microseconds of boot, so the
    // line is never left floating during the (possibly long) blocking switch
    // check below — otherwise the first pixel latches noise and stays green.
    ledInit();

    // Load the persisted settings from the AT24 EEPROM (I2C1 bus up first).
    boardI2C1Init();
    settingsInit();
    tempSensorInit();

    // Bring up the OLED (independent I2C2 bus) BEFORE the mode selector so it
    // can show the pending mode while the switch is held.
    oledReady = oledInit();
    functionMode = checkFunctionSwitch(oledReady);

    // servoInit() deferred: the servo outputs (PC6/PC7) are a reserved seam,
    // not driven yet — servo_out.cpp is excluded via build_src_filter so the
    // Servo library + its TIM16 setup are not linked. Re-add both when needed.

    // Resolve the bus baud rate: the stored value through the whitelist
    // (invalid -> 9600), and SET_ID / FACTORY_RESET modes always force the
    // default so the function switch remains a recovery path.
    uint32_t baud = settingsBaudAllowed(settings().baudRate) ? settings().baudRate : DEFAULT_BAUD_RATE;
    if (functionMode == FUNC_SW_SET_ID || functionMode == FUNC_SW_FACTORY_RESET)
    {
        baud = DEFAULT_BAUD_RATE;
    }
    rs485PortBegin(baud);

    // Start the Modbus server with the stored ID, or special ID (246) for
    // SET_ID mode, then publish the settings into the registers.
    modbusServerInit(functionMode == FUNC_SW_SET_ID ? DEFAULT_IDENTIFIER - 1 : settings().identifier,
                     baud);
    mbSettingsToRegisters();

    // App modules register their Modbus reactions, then the shadows are
    // seeded from the just-published values so boot never fires a handler.
    // display_control owns the RUN screen (clears the boot mode indicator
    // and renders the reg-60 number) only when RUN runs with an OLED.
    opsInit();
    latchControlInit();
    ledControlInit();
    displayControlInit(functionMode == FUNC_SW_RUN && oledReady);
    servoControlInit();
    otaControlInit();
    diagControlInit(oledReady, (uint8_t)functionMode);
    mbWatchSeedShadows();

    // Start the independent watchdog LAST — after the (up to 15s) blocking
    // function-switch measurement — now that the loop is guaranteed
    // non-blocking. Cannot be stopped once started.
    IWatchdog.begin(WATCHDOG_TIMEOUT_MS * 1000UL);
}

void appRun()
{
    IWatchdog.reload();

    // Fixed tick pipeline. Safety-relevant ticks run unconditionally BEFORE
    // mode logic, so no mode can starve latch tracking or the LED
    // max-on-time enforcement.
    modbusServerTick();     // poll the bus + dispatch registered handlers

    // Sample the time AFTER the Modbus tick: handlers stamp state with a
    // fresh millis(), so an earlier sample would make now - timestamp
    // underflow and instantly expire delays/max-on-time in the same loop.
    uint32_t now = millis();
    latchControlTick(now);  // pulse FSM + lock-state tracking + reg 40
    ledControlTick(now);    // max-on-time enforcement + statistics
    otaControlTick(now);    // OTA session inactivity timeout
    diagControlTick(now);   // uptime/health publishing

    switch (functionMode)
    {
        case FUNC_SW_DEMO:          runDemoMode();          break;
        case FUNC_SW_SET_ID:        runSetIdMode();         break;
        case FUNC_SW_FACTORY_RESET: runFactoryResetMode();  break;
        case FUNC_SW_RUN:           runNormalMode();        break;
        default:                                            break;
    }
}

// ---------------------------------------------------------------------------
// Operating-mode handlers
// ---------------------------------------------------------------------------

// Demo mode: rainbow ripple + OLED counter stepped by the function switch
static void runDemoMode()
{
    static uint32_t lastDemoFrame = 0;
    static uint16_t rainbowPhase = 0;
    static uint8_t oledCounter = 0;
    static bool lastSwitchPressed = false;
    static uint32_t lastSwitchEdge = 0;
    static bool inited = false;

    if (!inited)
    {
        inited = true;
        oledCounter = (uint8_t)(settings().identifier % 100); // start from the device ID
    }

    bool switchPressed = boardFunctionSwitchPressed();
    if (switchPressed && !lastSwitchPressed && (millis() - lastSwitchEdge >= DEMO_SWITCH_DEBOUNCE_MS))
    {
        lastSwitchEdge = millis();
        oledCounter = (oledCounter + 1) % 100;

        // Latch bench test: each press fires a full 500ms pulse ignoring the
        // sense pin (always energizes, fixed max duration), so the solenoid
        // drive can be tested regardless of latch state. Still safety-capped
        // at 500ms with the 2s cooldown between pulses.
        latchRequestUnlock(LATCH_MAX_UNLOCK_TIME, 0, /*enableCoilToSync=*/0, /*ignoreSense=*/true);
    }
    lastSwitchPressed = switchPressed;

    if (millis() - lastDemoFrame >= DEMO_FRAME_MS)
    {
        lastDemoFrame = millis();
        rainbowPhase += 1;

        updateOledCounter(oledCounter);
        ledShowRainbowRipple(rainbowPhase);
    }
}

// Set ID mode: blink blue for identification, show the ID on the OLED, and
// let the operator assign it with the function switch (tap = +1, hold = save).
// The device still enumerates at the special Modbus ID 246 so a master can
// discover and assign it too — the button is an additional path, not a
// replacement.
static void runSetIdMode()
{
    static PeriodicTimer blinkTimer{ROUTINE_BLINK_SET_ID_MS};
    static bool ledOn = false;
    static bool inited = false;
    static uint16_t selectedId = SETID_ID_MIN;
    static bool lastPressed = false;
    static uint32_t pressStart = 0;
    static bool saveTriggered = false;

    uint32_t now = millis();

    if (!inited)
    {
        inited = true;
        selectedId = settings().identifier;
        if (selectedId < SETID_ID_MIN || selectedId > SETID_ID_MAX)
        {
            selectedId = SETID_ID_MIN; // fresh device (247) or a Modbus-set ID > 99
        }
        lastPressed = boardFunctionSwitchPressed(); // don't count the mode-select hold as a tap
        updateOledSetId(selectedId);
    }

    // Blue identification blink (unchanged)
    if (blinkTimer.due(now))
    {
        ledOn = !ledOn;
        for (int i = 0; i < LED_NUM; i++)
        {
            ledSetAllPixels(i, ledColor(0, 0, (ledOn ? SET_ID_BLINK_BLUE : 0))); // Blue or off
        }
    }

    // Single-button interaction: tap = +1 (wrap), hold = save & reboot.
    bool pressed = boardFunctionSwitchPressed();

    if (pressed && !lastPressed)
    {
        pressStart = now;
        saveTriggered = false;
    }

    if (pressed && !saveTriggered && (now - pressStart >= SETID_SAVE_HOLD_MS))
    {
        saveTriggered = true; // fire once
        if (oledReady)
        {
            oledPrint("SAVED", 3);
        }
        settingsEdit().identifier = selectedId;
        settingsSave();
        opsSystemReset(); // reboots into RUN at the new ID (forces MOSFET low first)
    }

    if (!pressed && lastPressed)
    {
        uint32_t duration = now - pressStart;
        if (!saveTriggered && duration >= SETID_TAP_MIN_MS && duration < SETID_SAVE_HOLD_MS)
        {
            selectedId++;
            if (selectedId > SETID_ID_MAX)
            {
                selectedId = SETID_ID_MIN;
            }
            updateOledSetId(selectedId);
        }
    }

    lastPressed = pressed;
}

// Factory reset mode: solid red on all LEDs for 5 seconds, then reset.
// Non-blocking: Modbus keeps being served during the warning window.
static void runFactoryResetMode()
{
    static bool armed = false;
    static uint32_t armedAtMs = 0;
    static uint16_t lastShownSecs = 0xFFFF;

    if (!armed)
    {
        armed = true;
        armedAtMs = millis();
        for (int i = 0; i < LED_NUM; i++)
        {
            ledSetAllPixels(i, ledColor(FACTORY_RESET_RED, 0, 0)); // Red
        }
    }

    uint32_t elapsed = millis() - armedAtMs;
    if (elapsed >= FACTORY_RESET_HOLD_MS)
    {
        settingsFactoryReset(false);    // Restore defaults (including ID)
        opsSystemReset();               // Perform software reset
    }

    // Countdown (ceil of remaining seconds), rendered only when it changes.
    uint16_t secsLeft = (uint16_t)((FACTORY_RESET_HOLD_MS - elapsed + 999) / 1000);
    if (secsLeft != lastShownSecs)
    {
        lastShownSecs = secsLeft;
        updateOledFactoryCountdown(secsLeft);
    }
}

// Normal operation: RUN LED heartbeat and temperature publishing.
// The OLED in RUN belongs to display_control (cleared at init, driven by
// reg 60 + coil 1010) — this mode never touches it.
static void runNormalMode()
{
    // Fast heartbeat signals a storage fault (AT24 absent -> nothing persists)
    static PeriodicTimer blinkTimer{settingsStorageOk() ? ROUTINE_BLINK_RUN_MS : STORAGE_FAULT_BLINK_MS};
    static PeriodicTimer sensorTimer{ROUTINE_SENSOR_READ_MS / 2};
    static bool runLedOn = false;
    static bool readBoardNext = false;

    // Routine blink for run LED
    if (blinkTimer.due(millis()))
    {
        runLedOn = !runLedOn;
        boardSetRunLed(runLedOn);
    }

    // Routine sensor read: alternate the two sensors so each is refreshed
    // once per ROUTINE_SENSOR_READ_MS with only one ~10ms I2C read per tick.
    if (sensorTimer.due(millis()))
    {
        int16_t centiC = 0; // degrees C x100, integer (no float)
        if (readBoardNext)
        {
            bool ok = tempReadBoardCenti(centiC);
            if (ok)
            {
                mbRegWrite(MB_REG_BOARD_TEMP, (uint16_t)centiC);
            }
            diagReportSensor(1, ok); // repeated failures -> 0x8000 sentinel
        }
        else
        {
            bool ok = tempReadRoomCenti(centiC);
            if (ok)
            {
                mbRegWrite(MB_REG_ROOM_TEMP, (uint16_t)centiC);
            }
            diagReportSensor(0, ok);
        }
        readBoardNext = !readBoardNext;
    }
}

// ---------------------------------------------------------------------------
// OLED helpers
// ---------------------------------------------------------------------------

static void updateOledCounter(uint8_t value)
{
    if (!oledReady)
    {
        return;
    }

    oledPrintLargeNumber(value);
}

static void updateOledSetId(uint16_t id)
{
    if (!oledReady)
    {
        return;
    }

    oledPrintTitledNumber("SET ID", id);
}

static void updateOledFactoryCountdown(uint16_t secs)
{
    if (!oledReady)
    {
        return;
    }

    oledPrintTitledNumber("FACTORY RESET", secs);
}
