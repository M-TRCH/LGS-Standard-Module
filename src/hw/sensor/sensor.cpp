#include "hw/sensor.h"

SensirionI2CSts4x sts4x;

void sensorInit()
{
    // Internal devices live on I2C1 (default Wire instance)
    Wire.setSDA(HW_I2C1_SDA_PIN);
    Wire.setSCL(HW_I2C1_SCL_PIN);
    Wire.begin();
    sts4x.begin(Wire, ADDR_STS4X_ALT);
}

bool sensorReadTemperature(float &temperatureC)
{
    uint16_t error = sts4x.measureHighPrecision(temperatureC);
    return (error == 0);
}
