
#include "system.h"

HardwareSerial Serial3(RX3_PIN, TX3_PIN);
RS485Class rs485(Serial, DUMMY_PIN, TX_PIN, RX_PIN);        // Initialize RS485 with Serial and Serial3
RS485Class rs4853(Serial3, DUMMY_PIN, TX3_PIN, RX3_PIN);     

// Global variables
bool run_led_state = false;

// Debug level configuration
DebugLevel debugLevel = DEBUG_BASIC;

void sysInit()
{
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
    digitalWrite(MOSFET_PIN, HIGH);
    delay(300);

    // Initialize pins to default states
    digitalWrite(LED_RUN_PIN, LOW);
    digitalWrite(MOSFET_PIN, LOW);

    Serial.setRx(RX_PIN);
    Serial.setTx(TX_PIN);
    Serial.begin(DEBUG_BAUD);
    Serial3.begin(MODBUS_BAUD);

    // For testing purposes
    while(1)
    {
        Serial.printf("Sensor reading: %d\n", isLatchLocked() ? 1 : 0);
    }

    // Wait for Serial to be ready
    uint32_t startup = millis();
    while(millis() - startup < 2000 && false)   // Set to true to enable waiting for Serial
    {
        PRINT(DEBUG_BASIC, F("."));
        delay(200);
    }
    // PRINT(DEBUG_BASIC, F("\n")); 
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
