#ifndef VERSION_H
#define VERSION_H

/*  @file version.h
 *  @brief Compile-time device identity, reported via Modbus registers 0-2.
 *
 *  These are constants baked into the firmware image (never stored in
 *  EEPROM), so the reported versions always match the running build.
 *  Each value can be overridden per PlatformIO environment with a -D
 *  build flag, e.g. -DDEVICE_TYPE=20 for a narcotic-cabinet variant.
 */

// Device type: 10=STANDARD, 20=NARCOTIC, 30=LITE, 40=DELIVERY
#ifndef DEVICE_TYPE
#define DEVICE_TYPE     40
#endif

// Firmware version, ddmmy encoding: dd=day, mm=month, y=last digit of year
// (e.g. 16076 = 16/07/2026)
#ifndef FW_VERSION
#define FW_VERSION      16076
#endif

// Hardware version, mnp encoding: m=major, n=minor, p=production run
// 500 = board revision R5.0 (STM32G070CBT6)
#ifndef HW_VERSION
#define HW_VERSION      500
#endif

#endif // VERSION_H
