#include "hal/hal_sensor.h"
#include <Wire.h>
#include <SensirionI2CSts4x.h>

// Sensor driver instance is owned by this module (not exposed globally).
static SensirionI2CSts4x sts4x;

void sensorInit()
{
    Wire.setSDA(SDA1_PIN);
    Wire.setSCL(SCL1_PIN);
    Wire.begin();
    sts4x.begin(Wire, ADDR_STS4X_ALT);
    LOG_DEBUG_SYS(F("[SENSOR] Temperature sensor HAL initialized\n"));
}

bool sensorReadTemperature(float &temperatureC)
{
    uint16_t error = sts4x.measureHighPrecision(temperatureC);
    return (error == 0);
}
