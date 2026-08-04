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
#define DEVICE_TYPE     20
#endif

// Firmware version, semantic: major * 10000 + minor * 100 + patch.
// 30100 = v3.1.0, 20002 = v2.0.2. Ranges are major 0-6, minor and patch 0-99,
// which is what a uint16 holding register allows.
//
// Replaced a ddmmy date code, which could not be compared: 04/08/2026 encoded
// as 4086, numerically *below* 17076 from three weeks earlier, because the
// leading zero of a single-digit day is lost. Anything asking "is this newer"
// got the wrong answer for a third of the days in a month. Release filenames
// keep the date as a suffix, where it reads as a date and never as a number.
#ifndef FW_VERSION
#define FW_VERSION      30100
#endif

// Hardware version, mnp encoding: m=major, n=minor, p=production run
// 500 = board revision R5.0 (STM32G070CBT6)
#ifndef HW_VERSION
#define HW_VERSION      500
#endif

#endif // VERSION_H
