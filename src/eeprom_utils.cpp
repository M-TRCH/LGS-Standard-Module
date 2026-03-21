
#include "eeprom_utils.h"

EepromConfig_t eepromConfig;
EepromConfig_t eepromConfig_cache;
EepromConfig_t eepromConfig_default = 
{
    .isFirstBootExceptID = false,                   // Indicates if first boot except ID
    .isFirstBoot = false,                           // Indicates if first boot
    .deviceType = DEFAULT_DEVICE_TYPE,              // Default device type
    .fwVersion = DEFAULT_FW_VERSION,                // Default firmware version 1.00
    .hwVersion = DEFAULT_HW_VERSION,                // Default hardware version
    .baudRate = DEFAULT_BAUD_RATE,                  // Default baud rate
    .identifier = DEFAULT_IDENTIFIER,               // Default Modbus identifier
    .led_brightness = DEFAULT_LED_BRIGHTNESS,       // Default brightness for 8 LEDs
    .led_r = DEFAULT_LED_R,                         // Default red values for 8 LEDs
    .led_g = DEFAULT_LED_G,                         // Default green values for 8 LEDs
    .led_b = DEFAULT_LED_B,                         // Default blue values for 8 LEDs
    .maxOnTime = DEFAULT_LED_MAX_ON_TIME,           // Default max on time for 8 LEDs
    .unlockDelayTime = DEFAULT_UNLOCK_DELAY_TIME    // Default unlock delay time
};

// ---- AT24C32D Low-Level I2C Functions ----

// Write up to one page within the same page boundary
static bool at24c32d_writePage(uint16_t memAddr, const uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(AT24C32D_ADDR);
    Wire.write((uint8_t)(memAddr >> 8));    // MSB of memory address
    Wire.write((uint8_t)(memAddr & 0xFF));  // LSB of memory address
    for (uint8_t i = 0; i < len; i++)
        Wire.write(buf[i]);
    uint8_t err = Wire.endTransmission();
    delay(5); // AT24C32D write cycle time (max 5ms)
    return (err == 0);
}

// Write multiple bytes using page writes (respects 32-byte page boundaries)
static bool at24c32d_writeBytes(uint16_t memAddr, const uint8_t *buf, uint16_t len)
{
    uint16_t written = 0;
    while (written < len)
    {
        uint8_t pageOffset = (uint8_t)((memAddr + written) % AT24C32D_PAGE_SIZE);
        uint8_t pageRemain = AT24C32D_PAGE_SIZE - pageOffset;
        uint8_t toWrite    = ((len - written) < pageRemain) ? (uint8_t)(len - written) : pageRemain;

        if (!at24c32d_writePage(memAddr + written, buf + written, toWrite))
            return false;
        written += toWrite;
    }
    return true;
}

// Read multiple bytes sequentially (up to 32 bytes per requestFrom)
static bool at24c32d_readBytes(uint16_t memAddr, uint8_t *buf, uint16_t len)
{
    Wire.beginTransmission(AT24C32D_ADDR);
    Wire.write((uint8_t)(memAddr >> 8));
    Wire.write((uint8_t)(memAddr & 0xFF));
    if (Wire.endTransmission() != 0) return false;

    uint16_t remaining = len;
    uint16_t offset = 0;
    while (remaining > 0)
    {
        uint8_t chunk = (remaining > 32) ? 32 : (uint8_t)remaining;
        Wire.requestFrom((uint8_t)AT24C32D_ADDR, chunk);
        uint8_t received = 0;
        while (Wire.available() && received < chunk)
        {
            buf[offset++] = Wire.read();
            received++;
        }
        if (received != chunk) return false;
        remaining -= chunk;
    }
    return true;
}

// ---- EEPROM Config Functions ----

void loadEepromConfig() 
{
    if (!at24c32d_readBytes(AT24C32D_CONFIG_ADDR, (uint8_t *)&eepromConfig, sizeof(EepromConfig_t)))
    {
        LOG_ERROR_EEPROM(F("[EEPROM] Failed to read from AT24C32D - loading defaults\n"));
        eepromConfig = eepromConfig_default;
    }
    eepromConfig_cache = eepromConfig; // keep a copy for change detection
    LOG_DEBUG_EEPROM(F("[EEPROM] Configuration loaded from AT24C32D\n"));
}

void clearEeprom(bool whileRunning) 
{
    LOG_INFO_EEPROM(F("[EEPROM] Clearing AT24C32D...\n"));
    uint8_t ffBuf[AT24C32D_PAGE_SIZE];
    memset(ffBuf, 0xFF, AT24C32D_PAGE_SIZE);

    uint16_t totalLen = sizeof(EepromConfig_t);
    uint16_t addr = AT24C32D_CONFIG_ADDR;
    while (totalLen > 0)
    {
        uint8_t toWrite = (totalLen > AT24C32D_PAGE_SIZE) ? AT24C32D_PAGE_SIZE : (uint8_t)totalLen;
        if (!at24c32d_writePage(addr, ffBuf, toWrite))
        {
            LOG_ERROR_EEPROM(F("[EEPROM] Failed to clear AT24C32D\n"));
            return;
        }
        addr += toWrite;
        totalLen -= toWrite;
    }
    LOG_INFO_EEPROM(F("[EEPROM] AT24C32D cleared successfully\n"));
    while (whileRunning)
    {
        LOG_VERBOSE_EEPROM(F("."));
        delay(3000);
    }
}

bool saveEepromConfig() 
{
    // Only write if data changed
    if (memcmp(&eepromConfig, &eepromConfig_cache, sizeof(EepromConfig_t)) != 0) 
    {
        if (!at24c32d_writeBytes(AT24C32D_CONFIG_ADDR, (const uint8_t *)&eepromConfig, sizeof(EepromConfig_t)))
        {
            LOG_ERROR_EEPROM(F("[EEPROM] Failed to save configuration to AT24C32D\n"));
            return false;
        }
        eepromConfig_cache = eepromConfig; // update cache
        LOG_INFO_EEPROM(F("[EEPROM] Configuration saved to AT24C32D\n"));
        return true;
    }
    LOG_DEBUG_EEPROM(F("[EEPROM] No changes detected, skipping save\n"));
    return false;
}

void printEepromConfig()
{
    Serial.println(F("\n[EEPROM] ===== Configuration ====="));
    Serial.println("[EEPROM]  isFirstBoot          : " + String(eepromConfig.isFirstBoot));
    Serial.println("[EEPROM]  isFirstBootExceptID  : " + String(eepromConfig.isFirstBootExceptID));
    Serial.println("[EEPROM]  deviceType           : " + String(eepromConfig.deviceType));
    Serial.println("[EEPROM]  fwVersion            : " + String(eepromConfig.fwVersion));
    Serial.println("[EEPROM]  hwVersion            : " + String(eepromConfig.hwVersion));
    Serial.println("[EEPROM]  baudRate             : " + String(eepromConfig.baudRate));
    Serial.println("[EEPROM]  identifier           : " + String(eepromConfig.identifier));
    Serial.println("[EEPROM]  unlockDelayTime      : " + String(eepromConfig.unlockDelayTime));
    Serial.println(F("[EEPROM]  --- LED Config ---"));
    Serial.println(F("[EEPROM]  # | brightness |   R |   G |   B | maxOnTime"));
    for (int i = 0; i < 8; i++)
    {
        Serial.print("[EEPROM]  " + String(i + 1) + " | ");
        Serial.print(String(eepromConfig.led_brightness[i]) + "         | ");
        Serial.print(String(eepromConfig.led_r[i]) + "   | ");
        Serial.print(String(eepromConfig.led_g[i]) + "   | ");
        Serial.print(String(eepromConfig.led_b[i]) + "   | ");
        Serial.println(String(eepromConfig.maxOnTime[i]));
    }
    Serial.println(F("[EEPROM] ==============================\n"));
}

void eepromInit() 
{
    // Load configuration from EEPROM
    loadEepromConfig(); 

    // Check if first boot
    // write default values to EEPROM
    if (eepromConfig.isFirstBoot == true) 
    {
        LOG_INFO_EEPROM(F("[EEPROM] First boot detected - loading defaults\n"));
        eepromConfig = eepromConfig_default;
        if (saveEepromConfig())                         // Save updated config to EEPROM
        {
            NVIC_SystemReset();                         // Perform software reset only if save succeeded
        }
        LOG_ERROR_EEPROM(F("[EEPROM] First boot save failed - continuing with defaults in RAM\n"));
    }

    // write default values to EEPROM except ID
    if (eepromConfig.isFirstBootExceptID == true) 
    {
        LOG_INFO_EEPROM(F("[EEPROM] First boot (except ID) detected - loading defaults\n"));
        uint16_t saved_id = eepromConfig.identifier;    // save current ID
        eepromConfig = eepromConfig_default;
        eepromConfig.identifier = saved_id;             // restore ID
        if (saveEepromConfig())                         // Save updated config to EEPROM
        {
            NVIC_SystemReset();                         // Perform software reset only if save succeeded
        }
        LOG_ERROR_EEPROM(F("[EEPROM] First boot (except ID) save failed - continuing with defaults in RAM\n"));
    }
}