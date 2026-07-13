#include "drivers/board_io.h"

void boardIoInit()
{
    pinMode(HW_LED_BUILTIN_PIN, OUTPUT);
    pinMode(HW_LATCH_TRIGGER_PIN, OUTPUT);
    pinMode(HW_LATCH_CHECK_PIN, INPUT_PULLUP);
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
