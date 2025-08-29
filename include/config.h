
#ifndef CONFIG_H
#define CONFIG_H

// Modbus configuration
#define MODBUS_SERIAL       0x00
#define MODBUS_SERIAL3      0x01
#define MODBUS_OUTPUT       MODBUS_SERIAL3  // Select the physical output

// Default configuration values
#define DEFAULT_DEVICE_TYPE     0
#define DEFAULT_FW_VERSION      28085   // ddmmy dd=day, mm=month, y=year (e.g. 28085 = 28/08/2025)
#define DEFAULT_HW_VERSION      401     // mnp m=major, n=minor, p=production time
#define DEFAULT_BAUD_RATE       9600
#define DEFAULT_IDENTIFIER      99      // for Testing
#define DEFAULT_LED_BRIGHTNESS  {20, 20, 20, 20, 20, 20, 20, 20}
#define DEFAULT_LED_R           {255,   0,      0,      255,    0,      255,    255,    255}
#define DEFAULT_LED_G           {0,     255,    0,      255,    255,    0,      128,    255}
#define DEFAULT_LED_B           {0,     0,      255,    0,      255,    255,    0,      235}

#endif