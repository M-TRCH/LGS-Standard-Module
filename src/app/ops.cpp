#include "app/ops.h"
#include "drivers/board_io.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"
#include "svc/settings.h"

namespace {

// Write data to EEPROM (Addr.503)
void onWriteToEeprom(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbRegistersToSettings(true);
    opsSystemReset();
}

// Factory reset arm coil (Addr.500) + apply coils (501/502)
void onFactoryReset(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;

    // Address 501: factory reset except ID
    if (mbCoilRead(MB_COIL_APPLY_FACTORY_RESET_EXCEPT_ID))
    {
        settingsFactoryReset(true);
        opsSystemReset();
    }
    // Address 502: factory reset all data
    if (mbCoilRead(MB_COIL_APPLY_FACTORY_RESET_ALL_DATA))
    {
        settingsFactoryReset(false);
        opsSystemReset();
    }
}

// Software reset (Addr.504)
void onSoftwareReset(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    opsSystemReset();
}

} // namespace

void opsInit()
{
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_WRITE_TO_EEPROM, onWriteToEeprom);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_FACTORY_RESET, onFactoryReset);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_SOFTWARE_RESET, onSoftwareReset);
}

void opsSystemReset()
{
    // The latch pulse spans loop iterations once it is a state machine, so a
    // reset request can arrive mid-pulse. Force the MOSFET off before the
    // GPIO goes hi-Z during reset, otherwise the gate would float.
    boardLatchMosfetSet(false);
    NVIC_SystemReset();
    while (true) {} // unreachable; satisfies noreturn
}
