#include "drivers/temp_sensor.h"
#include <Wire.h>
#include <SensirionI2CSts4x.h>
#include "board.h"

// Two STS40 variants on the internal I2C1 bus, distinguished by address:
// CD1B -> 0x46 (library default ADDR_STS4X), AD1B -> 0x44 (ADDR_STS4X_ALT).
static SensirionI2CSts4x stsRoom;   // STS40-CD1B-R3, room temperature
static SensirionI2CSts4x stsBoard;  // STS40-AD1B-R3, board temperature

void tempSensorInit()
{
    // Internal devices live on I2C1 (default Wire instance)
    Wire.setSDA(HW_I2C1_SDA_PIN);
    Wire.setSCL(HW_I2C1_SCL_PIN);
    Wire.begin();

    stsRoom.begin(Wire, ADDR_STS4X);        // 0x46
    stsBoard.begin(Wire, ADDR_STS4X_ALT);   // 0x44
}

bool tempReadRoom(float &temperatureC)
{
    return (stsRoom.measureHighPrecision(temperatureC) == 0);
}

bool tempReadBoard(float &temperatureC)
{
    return (stsBoard.measureHighPrecision(temperatureC) == 0);
}
