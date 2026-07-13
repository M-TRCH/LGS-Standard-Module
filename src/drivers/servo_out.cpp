#include "drivers/servo_out.h"
#include <Servo.h>
#include "HW_config.h"

static Servo servos[SERVO_COUNT];

static const uint8_t servoPins[SERVO_COUNT] = { HW_SERVO1_PIN, HW_SERVO2_PIN };

void servoInit()
{
    for (uint8_t i = 0; i < SERVO_COUNT; i++)
    {
        servos[i].attach(servoPins[i]);
    }
}

void servoWrite(uint8_t index, uint8_t angle)
{
    if (index >= SERVO_COUNT)
    {
        return;
    }
    if (angle > SERVO_MAX_ANGLE)
    {
        angle = SERVO_MAX_ANGLE;
    }
    if (!servos[index].attached())
    {
        servos[index].attach(servoPins[index]);
    }
    servos[index].write(angle);
}

void servoWriteMicroseconds(uint8_t index, uint16_t microseconds)
{
    if (index >= SERVO_COUNT)
    {
        return;
    }
    if (!servos[index].attached())
    {
        servos[index].attach(servoPins[index]);
    }
    servos[index].writeMicroseconds(microseconds);
}

void servoDetach(uint8_t index)
{
    if (index >= SERVO_COUNT)
    {
        return;
    }
    servos[index].detach();
}
