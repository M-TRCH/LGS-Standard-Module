/**
 * OTA Test: App2 (Ver2)
 * =====================
 * Flash Region : 0x08008000 - 0x0800FFFF (second 32KB)
 * Behavior     : LED_RUN blinks fast (200ms interval)
 * OTA Trigger  : Hold FUNC_SW > 3 seconds -> jump back to App1
 * 
 * Build : pio run -e ota_app2
 * Flash : Use STM32CubeProgrammer to write firmware.bin at 0x08008000
 */

#include <Arduino.h>

// ---- Pin Definitions (standalone, no system.h dependency) ----
#define LED_RUN_PIN     PA15
#define FUNC_SW_PIN     PA0
#define RX_PIN          PA10
#define TX_PIN          PA9

// ---- OTA Configuration ----
#define APP1_ADDRESS    0x08000000U
#define BLINK_INTERVAL  200     // ms - fast blink = App2

typedef void (*pFunction)(void);

/**
 * Jump to application at the given flash address.
 * (Same implementation as App1 - could be a shared library)
 */
void jumpToApp(uint32_t address)
{
    uint32_t appStack = *(__IO uint32_t*)address;
    pFunction appEntry = (pFunction)(*(__IO uint32_t*)(address + 4));

    Serial.end();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    __disable_irq();
    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    SCB->VTOR = address;
    __set_MSP(appStack);
    appEntry();
    while (1);
}

// ================================================================
void setup()
{
    // Safety: ensure vector table points to App2's region
    // (Already set by jump function, but reinforce in case of edge cases)
    SCB->VTOR = 0x08008000;

    pinMode(LED_RUN_PIN, OUTPUT);
    pinMode(FUNC_SW_PIN, INPUT);

    Serial.setRx(RX_PIN);
    Serial.setTx(TX_PIN);
    Serial.begin(9600);

    Serial.println(F("\n==================================="));
    Serial.println(F("[App2] Ver2 - OTA Test Application"));
    Serial.println(F("[App2] Flash : 0x08008000 (second 32KB)"));
    Serial.println(F("[App2] Blink : 200ms (fast)"));
    Serial.println(F("[App2] Hold FUNC_SW > 3s to jump back to App1"));
    Serial.println(F("===================================\n"));
}

void loop()
{
    // ---- Fast blink: visual indicator for App2 ----
    static uint32_t lastBlink = 0;
    static bool ledState = false;
    if (millis() - lastBlink >= BLINK_INTERVAL)
    {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_RUN_PIN, ledState);
    }

    // ---- FUNC_SW: jump back to App1 ----
    if (digitalRead(FUNC_SW_PIN) == LOW)
    {
        delay(50); // Debounce
        if (digitalRead(FUNC_SW_PIN) == LOW)
        {
            Serial.println(F("[App2] FUNC_SW pressed - hold > 3s to jump back..."));
            uint32_t pressStart = millis();

            while (digitalRead(FUNC_SW_PIN) == LOW)
            {
                // Solid LED while button is held
                digitalWrite(LED_RUN_PIN, HIGH);

                if (millis() - pressStart >= 3000)
                {
                    Serial.println(F("[App2] >>> Jumping back to App1..."));
                    delay(100);
                    jumpToApp(APP1_ADDRESS);
                }
            }
        }
    }
}
