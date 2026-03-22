/**
 * OTA Test: App1 (Ver1)
 * =====================
 * Flash Region : 0x08000000 - 0x08007FFF (first 32KB)
 * Behavior     : LED_RUN blinks slowly (1200ms interval)
 * OTA Trigger  : Hold FUNC_SW > 3 seconds -> jump to App2
 * 
 * Build  : pio run -e ota_app1
 * Upload : pio run -e ota_app1 --target upload
 */

#include <Arduino.h>

// ---- Pin Definitions (standalone, no system.h dependency) ----
#define LED_RUN_PIN     PA15
#define FUNC_SW_PIN     PA0
#define RX_PIN          PA10
#define TX_PIN          PA9

// ---- OTA Configuration ----
#define APP2_ADDRESS    0x08008000U
#define BLINK_INTERVAL  1200    // ms - slow blink = App1

typedef void (*pFunction)(void);

/**
 * Validate if a valid application exists at the given flash address.
 * Checks that the initial stack pointer (first word) points to valid RAM.
 * STM32F103C8: RAM = 0x20000000 - 0x20004FFF (20KB)
 */
bool isAppValid(uint32_t address)
{
    uint32_t sp = *(__IO uint32_t*)address;
    return (sp >= 0x20000000 && sp <= 0x20005000);
}

/**
 * Jump to application at the given flash address.
 * 
 * Sequence:
 *   1. Deinitialize peripherals & disable interrupts
 *   2. Relocate vector table (SCB->VTOR)
 *   3. Set MSP from target's vector table
 *   4. Jump to target's Reset_Handler
 */
void jumpToApp(uint32_t address)
{
    uint32_t appStack = *(__IO uint32_t*)address;
    pFunction appEntry = (pFunction)(*(__IO uint32_t*)(address + 4));

    // Deinitialize peripherals
    Serial.end();

    // Disable SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    // Disable all NVIC interrupts and clear pending
    __disable_irq();
    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    // Set vector table and stack pointer for target application
    SCB->VTOR = address;
    __set_MSP(appStack);

    // Jump to application Reset_Handler
    appEntry();

    // Should never reach here
    while (1);
}

// ================================================================
void setup()
{
    pinMode(LED_RUN_PIN, OUTPUT);
    pinMode(FUNC_SW_PIN, INPUT);

    Serial.setRx(RX_PIN);
    Serial.setTx(TX_PIN);
    Serial.begin(9600);

    Serial.println(F("\n==================================="));
    Serial.println(F("[App1] Ver1 - OTA Test Application"));
    Serial.println(F("[App1] Flash : 0x08000000 (first 32KB)"));
    Serial.println(F("[App1] Blink : 1200ms (slow)"));
    Serial.println(F("[App1] Hold FUNC_SW > 3s to jump to App2"));

    if (isAppValid(APP2_ADDRESS))
        Serial.println(F("[App1] App2 at 0x08008000 : VALID"));
    else
        Serial.println(F("[App1] App2 at 0x08008000 : NOT FOUND"));

    Serial.println(F("===================================\n"));
}

void loop()
{
    // ---- Slow blink: visual indicator for App1 ----
    static uint32_t lastBlink = 0;
    static bool ledState = false;
    if (millis() - lastBlink >= BLINK_INTERVAL)
    {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_RUN_PIN, ledState);
    }

    // ---- FUNC_SW: OTA jump trigger ----
    if (digitalRead(FUNC_SW_PIN) == LOW)
    {
        delay(50); // Debounce
        if (digitalRead(FUNC_SW_PIN) == LOW)
        {
            Serial.println(F("[App1] FUNC_SW pressed - hold > 3s to jump..."));
            uint32_t pressStart = millis();

            while (digitalRead(FUNC_SW_PIN) == LOW)
            {
                // Rapid blink while button is held
                digitalWrite(LED_RUN_PIN, ((millis() / 100) % 2) ? HIGH : LOW);

                if (millis() - pressStart >= 3000)
                {
                    if (isAppValid(APP2_ADDRESS))
                    {
                        Serial.println(F("[App1] >>> Jumping to App2..."));
                        delay(100); // Allow serial TX to flush
                        jumpToApp(APP2_ADDRESS);
                    }
                    else
                    {
                        Serial.println(F("[App1] Cannot jump - App2 not valid!"));
                        break;
                    }
                }
            }
        }
    }
}
