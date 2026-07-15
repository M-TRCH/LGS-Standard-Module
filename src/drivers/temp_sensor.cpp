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
    // I2C1 (default Wire instance) is brought up by boardI2C1Init().
    stsRoom.begin(Wire, ADDR_STS4X);        // 0x46
    stsBoard.begin(Wire, ADDR_STS4X_ALT);   // 0x44
}

namespace {
// STS4x datasheet: degC = (175 * ticks / 65535) - 45. In centidegrees and
// pure integer math: (17500 * ticks / 65535) - 4500. 17500 * 65535 fits in
// uint32 (~1.15e9), so no overflow and no float is pulled in.
bool readCenti(SensirionI2CSts4x &sensor, int16_t &centiC)
{
    uint16_t ticks = 0;
    if (sensor.measureHighPrecisionTicks(ticks) != 0)
    {
        return false;
    }
    centiC = (int16_t)((uint32_t)17500 * ticks / 65535) - 4500;
    return true;
}
} // namespace

bool tempReadRoomCenti(int16_t &centiC)
{
    return readCenti(stsRoom, centiC);
}

bool tempReadBoardCenti(int16_t &centiC)
{
    return readCenti(stsBoard, centiC);
}
