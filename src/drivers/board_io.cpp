#include "drivers/board_io.h"
#include <Wire.h>
#include <HardwareTimer.h>

// Latch-pulse guard on TIM7 (basic timer, free on this board: the core
// reserves TIM14 for Tone and TIM16 for Servo). Constructed lazily so the
// HAL is guaranteed initialized first.
static HardwareTimer& latchGuardTimer()
{
    static HardwareTimer timer(TIM7);
    return timer;
}

static void latchGuardTimeout()
{
    // ISR context: only the idempotent pin write and stopping the timer.
    digitalWrite(HW_LATCH_TRIGGER_PIN, LOW);
    latchGuardTimer().pause();
}

void boardLatchGuardArm(uint32_t timeoutMs)
{
    HardwareTimer &timer = latchGuardTimer();
    timer.pause();
    timer.setOverflow(timeoutMs * 1000, MICROSEC_FORMAT);
    timer.attachInterrupt(latchGuardTimeout);
    timer.setCount(0);
    timer.resume();
}

void boardLatchGuardDisarm()
{
    latchGuardTimer().pause();
}

void boardI2C1Init()
{
    Wire.setSDA(HW_I2C1_SDA_PIN);
    Wire.setSCL(HW_I2C1_SCL_PIN);
    Wire.begin();
}

// --- Input-current ADC, bare registers (RM0454) -------------------------
// analogRead() pulled in ~2.5 KB of HAL ADC init/calibration — for one
// channel read once a second. These few registers are the whole job, and
// the flash they return is what the next display variant will live in.
// Every flag wait is bounded: a dead ADC yields raw 0, never a hung boot
// (the 4 s IWDG is armed by then).

static bool adcReady = false;

static bool adcWaitSet(volatile uint32_t &reg, uint32_t mask)
{
    for (uint32_t guard = 100000; guard; guard--)
    {
        if (reg & mask)
        {
            return true;
        }
    }
    return false;
}

static void adcInit()
{
    RCC->APBENR2 |= RCC_APBENR2_ADCEN;
    (void)RCC->APBENR2;                          // settle the clock enable
    GPIOA->MODER |= GPIO_MODER_MODE6;            // PA6 analog (11) — reset
                                                 // default, made explicit

    ADC1->CR = ADC_CR_ADVREGEN;                  // regulator on
    delayMicroseconds(25);                       // > t_ADCVREG_STUP (20 us)
    ADC1->CR |= ADC_CR_ADCAL;                    // calibrate while disabled
    for (uint32_t guard = 100000; (ADC1->CR & ADC_CR_ADCAL) && guard; guard--) {}
    if (ADC1->CR & ADC_CR_ADCAL)
    {
        return;                                  // calibration never finished
    }

    ADC1->CFGR2 = (3u << ADC_CFGR2_CKMODE_Pos);  // PCLK/4 = 16 MHz, no async domain
    ADC1->SMPR = 5;                              // 39.5 cycles: gentle on the RC input
    ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_CCRDY;   // clear before arming
    ADC1->CR |= ADC_CR_ADEN;
    if (!adcWaitSet(ADC1->ISR, ADC_ISR_ADRDY))
    {
        return;
    }
    ADC1->CHSELR = ADC_CHSELR_CHSEL6;            // PA6 = ADC_IN6
    adcReady = adcWaitSet(ADC1->ISR, ADC_ISR_CCRDY);
}

static uint16_t adcReadRaw()
{
    if (!adcReady)
    {
        return 0;
    }
    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_EOS;
    ADC1->CR |= ADC_CR_ADSTART;
    if (!adcWaitSet(ADC1->ISR, ADC_ISR_EOC))     // ~3.3 us at 16 MHz
    {
        return 0;
    }
    return (uint16_t)ADC1->DR;                   // 12-bit right-aligned
}

void boardIoInit()
{
    pinMode(HW_LED_BUILTIN_PIN, OUTPUT);
    pinMode(HW_LATCH_TRIGGER_PIN, OUTPUT);
    pinMode(HW_LATCH_CHECK_PIN, INPUT);   // active-low sense; board provides the pull externally
    pinMode(HW_FUNCTION_SWITCH_PIN, INPUT);
    // Internal pull-up is what keeps one image valid on both boards: R5.1
    // adds its own 10k, R5.0 leaves the pin unrouted and would float.
    pinMode(HW_FUNCTION_SWITCH2_PIN, INPUT_PULLUP);
    adcInit();

    boardSetRunLed(false);
    boardLatchMosfetSet(false);
}

void boardSetRunLed(bool on)
{
    digitalWrite(HW_LED_BUILTIN_PIN, on ? HIGH : LOW);
}

bool boardFunctionSwitchPressed()
{
    // KEY1 (PA7) and its R5.1 mirror SW3 (PC13) are one logical switch:
    // every consumer (boot mode select, DEMO, SET_ID) sees either.
    return (digitalRead(HW_FUNCTION_SWITCH_PIN) == LOW)
        || (digitalRead(HW_FUNCTION_SWITCH2_PIN) == LOW);
}

uint16_t boardInputCurrentMa()
{
    // INA180A4 (x200) across 3 mOhm -> 0.6 mV per mA; 3300 mV / 4096 LSB.
    // mA = raw * 3300 / 4096 / 0.6 = raw * 33000 / 24576. raw <= 4095 keeps
    // the product inside uint32 (135,135,000) and the result <= 5,498 in
    // uint16. Integer-only on purpose — same discipline as temp_sensor.cpp.
    const uint32_t raw = adcReadRaw();
    return (uint16_t)((raw * 33000UL) / 24576UL);
}

void boardLatchMosfetSet(bool on)
{
    digitalWrite(HW_LATCH_TRIGGER_PIN, on ? HIGH : LOW);
}

bool boardLatchSenseLow()
{
    return (digitalRead(HW_LATCH_CHECK_PIN) == LOW);
}
