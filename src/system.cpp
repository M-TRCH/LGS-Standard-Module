
#include "system.h"

HardwareSerial Serial3(RX3_PIN, TX3_PIN);
RS485Class rs485(Serial, DUMMY_PIN, TX_PIN, RX_PIN);        // Initialize RS485 with Serial and Serial3
RS485Class rs4853(Serial3, DUMMY_PIN, TX3_PIN, RX3_PIN);     
SensirionI2CSts4x sts4x;

// Global variables
bool run_led_state = false;

// Log configuration
LogLevel globalLogLevel = LOG_INFO;                     // Default log level
uint8_t enabledLogCategories = LOG_CAT_ALL;            // Enable all categories by default

void sysInit(LogLevel logLevel, uint8_t logCategories)
{
    // Set global log configuration
    globalLogLevel = logLevel;
    enabledLogCategories = logCategories;

    LOG_INFO_SYS(F("[SYSTEM] Initializing system...\n"));   

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

    // Wait for Serial to be ready
    uint32_t startup = millis();
    while(millis() - startup < 2000 && false)   // Set to true to enable waiting for Serial
    {
        LOG_VERBOSE_SYS(F("."));
        delay(200);
    }
    LOG_INFO_SYS(F("[SYSTEM] Initialization complete\n"));
    digitalWrite(LED_RUN_PIN, HIGH);
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
    return false; // Latch is unlocked
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