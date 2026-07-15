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

void boardIoInit()
{
    pinMode(HW_LED_BUILTIN_PIN, OUTPUT);
    pinMode(HW_LATCH_TRIGGER_PIN, OUTPUT);
    pinMode(HW_LATCH_CHECK_PIN, INPUT);   // active-low sense; board provides the pull externally
    pinMode(HW_FUNCTION_SWITCH_PIN, INPUT);

    boardSetRunLed(false);
    boardLatchMosfetSet(false);
}

void boardSetRunLed(bool on)
{
    digitalWrite(HW_LED_BUILTIN_PIN, on ? HIGH : LOW);
}

bool boardFunctionSwitchPressed()
{
    return (digitalRead(HW_FUNCTION_SWITCH_PIN) == LOW);
}

void boardLatchMosfetSet(bool on)
{
    digitalWrite(HW_LATCH_TRIGGER_PIN, on ? HIGH : LOW);
}

bool boardLatchSenseLow()
{
    return (digitalRead(HW_LATCH_CHECK_PIN) == LOW);
}
