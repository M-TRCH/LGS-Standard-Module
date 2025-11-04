
#include "system.h"

HardwareSerial Serial3(RX3_PIN, TX3_PIN);
RS485Class rs485(Serial, DUMMY_PIN, TX_PIN, RX_PIN);        // Initialize RS485 with Serial and Serial3
RS485Class rs4853(Serial3, DUMMY_PIN, TX3_PIN, RX3_PIN);     
SensirionI2CSts4x sts4x;

// Global variables
uint32_t lastRoutineBlink = 0;
uint32_t lastRoutineSetID = 0;
bool run_led_state = false;
bool set_id_state = false;
FunctionSwitchMode functionMode = FUNC_SW_RUN;

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
    
    // For testing purposes
    unlockLatch();

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
    if (digitalRead(SENSE_PIN) == LOW)
    {
        digitalWrite(MOSFET_PIN, HIGH);  // Activate MOSFET to unlock latch
             
        uint32_t startTime = millis();
        while (digitalRead(SENSE_PIN) == LOW)
        {
            if (millis() - startTime >= unlockTimeout)
            {
                break; // Exit if timeout reached
            }
        }
        digitalWrite(MOSFET_PIN, LOW);  // Deactivate MOSFET after timeout
        return true; // Latch is active
    }
    else
    {
        return false; // Latch is inactive
    }
    return false; // Latch is inactive
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
    uint8_t lastReportedSecond = 0;
    
    // Wait for switch release or max time
    while (digitalRead(FUNC_SW_PIN) == LOW && (millis() - pressStartTime) < maxWaitTime)
    {
        pressDuration = millis() - pressStartTime;
        
        // Report every second
        uint8_t currentSecond = pressDuration / 1000;
        if (currentSecond > lastReportedSecond)
        {
            lastReportedSecond = currentSecond;
            LOG_INFO_SYS("[SYSTEM] Switch pressed: " + String(currentSecond) + " seconds...\n");
        }
        delay(50);  // Small delay to reduce CPU usage
    }
    
    // Determine which mode based on press duration
    FunctionSwitchMode mode = FUNC_SW_RUN;

    if (pressDuration >= 9000)  // 9 seconds or more
    {
        mode = FUNC_SW_FACTORY_RESET;
        LOG_INFO_SYS(F("[SYSTEM] Function switch: FACTORY_RESET (>9s) detected\n"));
    }
    else if (pressDuration >= 3000)  // 3 seconds or more
    {
        mode = FUNC_SW_SET_ID;
        LOG_INFO_SYS(F("[SYSTEM] Function switch: SET_ID (>3s) detected\n"));
    }
    else if (pressDuration >= 500)  // 0.5 second or more
    {
        mode = FUNC_SW_DEMO;
        LOG_INFO_SYS(F("[SYSTEM] Function switch: DEMO (>0.5s) detected\n"));
    }
    else
    {
        // Released too quickly, treat as no press
        mode = FUNC_SW_RUN;
        LOG_DEBUG_SYS(F("[SYSTEM] Function switch released too quickly, continuing normal operation\n"));
    }
    
    // Wait a bit to ensure switch is fully released
    delay(100);
    
    return mode;
}

bool ON_ROUTINE_BLINK()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastRoutineBlink >= ROUTINE_BLINK_MS)
    {
        lastRoutineBlink = currentMillis;
        return true;
    }
    return false;
}

bool ON_ROUTINE_SET_ID()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastRoutineSetID >= ROUTINE_SET_ID_MS)
    {
        lastRoutineSetID = currentMillis;
        return true;
    }
    return false;
}