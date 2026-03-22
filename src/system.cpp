
#include "system.h"

HardwareSerial Serial3(RX3_PIN, TX3_PIN);
RS485Class rs485(Serial, DUMMY_PIN, TX_PIN, RX_PIN);        // Initialize RS485 with Serial and Serial3
RS485Class rs4853(Serial3, DUMMY_PIN, TX3_PIN, RX3_PIN);     
SensirionI2CSts4x sts4x;

// Global variables
uint32_t lastTimeRoutineBlink = 0;
uint32_t lastTimeRoutineDemo = 0;
uint32_t lastTimeRoutineSetID = 0;
uint32_t lastTimeSensorRead = 0;
bool blink_run_state = false;
bool blink_demo_state = false;
bool blink_set_id_state = false;
FunctionSwitchMode functionMode = FUNC_SW_RUN;
uint32_t lastTimeLatchLocked = 0;

// Log configuration
LogLevel globalLogLevel = LOG_INFO;                     // Default log level
uint8_t enabledLogCategories = LOG_CAT_ALL;            // Enable all categories by default

void sysInit(LogLevel logLevel, uint8_t logCategories)
{
    // Set global log configuration
    globalLogLevel = logLevel;
    enabledLogCategories = logCategories;

    LOG_INFO_SYS(F("\n[SYSTEM] Initializing system...\n"));   

    // Initialize pins
    pinMode(LED_RUN_PIN, OUTPUT);
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    pinMode(LED4_PIN, OUTPUT);
    pinMode(LED5_PIN, OUTPUT);
    pinMode(LED6_PIN, OUTPUT);
    pinMode(LED7_PIN, OUTPUT);
    pinMode(LED8_PIN, OUTPUT);
    pinMode(MOSFET_PIN, OUTPUT);
    pinMode(SENSE_PIN, INPUT_PULLUP);
    pinMode(FUNC_SW_PIN, INPUT);
    
    // Initialize pins to default states
    digitalWrite(LED_RUN_PIN, LOW);
    digitalWrite(MOSFET_PIN, LOW);

    // Initialize Serial interfaces
    Serial.setRx(RX_PIN);
    Serial.setTx(TX_PIN);
    Serial.begin(DEBUG_BAUD);
    Serial3.begin(MODBUS_BAUD);

    // Initialize I2C and STS4x sensor
    Wire.setSDA(SDA1_PIN);
    Wire.setSCL(SCL1_PIN);
    Wire.begin(); // Initialize I2C
    sts4x.begin(Wire, ADDR_STS4X_ALT);

    // Check function switch immediately after system init
    functionMode = checkFunctionSwitch();

    LOG_INFO_SYS(F("[SYSTEM] Initialization complete\n"));
}

bool isLatchLocked(int debounceDelay)
{
    if (digitalRead(SENSE_PIN) == LOW)
    {
        delay(debounceDelay); // Debounce delay
        if (digitalRead(SENSE_PIN) == LOW)
        {
            return true; // Latch is locked
        }
    }
    return false; // Latch is inactive
}

bool unlockLatch(int unlockTimeout)
{
    // Safety check: enforce maximum unlock time
    if (unlockTimeout > LATCH_MAX_UNLOCK_TIME)
    {
        LOG_WARNING_SYS("[SYSTEM] Unlock timeout " + String(unlockTimeout) + "ms exceeds maximum " + String(LATCH_MAX_UNLOCK_TIME) + "ms, clamping to max\n");
        unlockTimeout = LATCH_MAX_UNLOCK_TIME;
    }

    // Safety check: prevent frequent unlocking
    static uint32_t lastUnlockTime = 0;
    uint32_t timeSinceLastUnlock = millis() - lastUnlockTime;
    
    if (lastUnlockTime != 0 && timeSinceLastUnlock < LATCH_MIN_INTERVAL)
    {
        LOG_WARNING_SYS("[SYSTEM] Unlock attempt blocked - only " + String(timeSinceLastUnlock) + "ms since last unlock (min " + String(LATCH_MIN_INTERVAL) + "ms)\n");
        return false; // Reject unlock attempt if too soon
    }

    if (digitalRead(SENSE_PIN) == LOW)
    {
        lastUnlockTime = millis();
        digitalWrite(MOSFET_PIN, HIGH);  // Activate MOSFET to unlock latch
        LOG_INFO_SYS("[SYSTEM] Latch unlocking for " + String(unlockTimeout) + "ms\n");
             
        uint32_t startTime = millis();
        while (digitalRead(SENSE_PIN) == LOW)
        {
            if (millis() - startTime >= unlockTimeout)
            {
                break; // Exit if timeout reached
            }
        }
        
        uint32_t actualDuration = millis() - startTime;
        digitalWrite(MOSFET_PIN, LOW);  // Deactivate MOSFET after timeout
        LOG_INFO_SYS("[SYSTEM] Latch unlocked for " + String(actualDuration) + "ms\n");
        
        return true; // Latch is active
    }
    else
    {
        LOG_DEBUG_SYS("[SYSTEM] Unlock attempt - latch already inactive\n");
        return false; // Latch is inactive
    }
}

FunctionSwitchMode checkFunctionSwitch(uint16_t maxWaitTime)
{
    LOG_DEBUG_SYS(F("[SYSTEM] Checking function switch...\n"));
    
    // Check if switch is pressed at startup (active LOW)
    if (digitalRead(FUNC_SW_PIN) == HIGH)
    {
        LOG_DEBUG_SYS(F("[SYSTEM] Function switch not pressed, continuing normal operation\n"));
        return FUNC_SW_RUN;  // Switch not pressed, continue normal operation
    }
    
    LOG_INFO_SYS(F("[SYSTEM] Function switch detected! Waiting for release...\n"));
    
    // Switch is pressed, measure how long it's held
    uint32_t pressStartTime = millis();
    uint32_t pressDuration = 0;
    uint32_t lastBlinkCycle = 0;
    
    // Wait for switch release or max time
    while (digitalRead(FUNC_SW_PIN) == LOW && (millis() - pressStartTime) < maxWaitTime)
    {
        pressDuration = millis() - pressStartTime;
        uint32_t currentCycle = pressDuration / 1000;
        
        if (currentCycle > lastBlinkCycle)
        {
            lastBlinkCycle = currentCycle;
            LOG_INFO_SYS("[SYSTEM] Switch pressed: " + String(currentCycle) + " seconds...\n");
        }
        
        // Rapid blink while held (200ms cycle)
        digitalWrite(LED_RUN_PIN, ((millis() / 100) % 2) ? HIGH : LOW);
        delay(50);
    }
    
    // Turn off LED after release
    digitalWrite(LED_RUN_PIN, LOW);
    
    // Press >= 3 seconds -> OTA mode
    FunctionSwitchMode mode = FUNC_SW_RUN;
    if (pressDuration >= 3000)
    {
        mode = FUNC_SW_OTA;
        LOG_INFO_SYS(F("[SYSTEM] Function switch: OTA mode detected\n"));
    }
    else
    {
        LOG_INFO_SYS("[SYSTEM] Function switch: No action (" + String(pressDuration) + "ms)\n");
    }
    
    delay(100);
    return mode;
}

bool ON_ROUTINE_BLINK_RUN()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastTimeRoutineBlink >= ROUTINE_BLINK_RUN_MS)
    {
        lastTimeRoutineBlink = currentMillis;
        blink_run_state = !blink_run_state;
        return true;
    }
    return false;
}

bool ON_ROUTINE_BLINK_DEMO()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastTimeRoutineDemo >= ROUTINE_BLINK_DEMO_MS)
    {
        lastTimeRoutineDemo = currentMillis;
        blink_demo_state = !blink_demo_state;
        return true;
    }
    return false;
}

bool ON_ROUTINE_BLINK_SET_ID()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastTimeRoutineSetID >= ROUTINE_BLINK_SET_ID_MS)
    {
        lastTimeRoutineSetID = currentMillis;
        blink_set_id_state = !blink_set_id_state;
        return true;
    }
    return false;
}

bool ON_ROUTINE_SENSOR_READ()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastTimeSensorRead >= ROUTINE_SENSOR_READ_MS)
    {
        lastTimeSensorRead = currentMillis;
        return true;
    }
    return false;
}