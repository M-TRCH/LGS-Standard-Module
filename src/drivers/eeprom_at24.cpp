#include "drivers/eeprom_at24.h"
#include <Wire.h>

namespace {
constexpr uint8_t  AT24_I2C_ADDR   = 0x50;
constexpr uint16_t AT24_SIZE_BYTES = 4096;
constexpr uint8_t  AT24_PAGE_SIZE  = 32;
// The Wire buffer is 32 bytes and a write transaction spends 2 of them on
// the memory address, so data moves in chunks of at most 30 bytes.
constexpr uint8_t  AT24_WRITE_CHUNK = 30;
constexpr uint8_t  AT24_READ_CHUNK  = 32;
constexpr uint32_t AT24_WRITE_CYCLE_TIMEOUT_MS = 10;

// After a write the device NAKs its address until the internal write
// cycle (~5ms) finishes; poll until it acks again.
bool at24WaitReady()
{
    uint32_t start = millis();
    do
    {
        Wire.beginTransmission(AT24_I2C_ADDR);
        if (Wire.endTransmission() == 0)
        {
            return true;
        }
    } while (millis() - start < AT24_WRITE_CYCLE_TIMEOUT_MS);
    return false;
}
} // namespace

bool at24Probe()
{
    Wire.beginTransmission(AT24_I2C_ADDR);
    return (Wire.endTransmission() == 0);
}

bool at24Read(uint16_t addr, uint8_t *buf, size_t len)
{
    if ((size_t)addr + len > AT24_SIZE_BYTES)
    {
        return false;
    }

    size_t done = 0;
    while (done < len)
    {
        uint16_t cur = addr + done;
        size_t n = len - done;
        if (n > AT24_READ_CHUNK)
        {
            n = AT24_READ_CHUNK;
        }

        Wire.beginTransmission(AT24_I2C_ADDR);
        Wire.write((uint8_t)(cur >> 8));
        Wire.write((uint8_t)(cur & 0xFF));
        if (Wire.endTransmission(false) != 0) // repeated start
        {
            return false;
        }
        if (Wire.requestFrom(AT24_I2C_ADDR, (uint8_t)n) != n)
        {
            return false;
        }
        for (size_t i = 0; i < n; i++)
        {
            buf[done + i] = (uint8_t)Wire.read();
        }
        done += n;
    }
    return true;
}

bool at24Write(uint16_t addr, const uint8_t *buf, size_t len)
{
    if ((size_t)addr + len > AT24_SIZE_BYTES)
    {
        return false;
    }

    size_t done = 0;
    while (done < len)
    {
        uint16_t cur = addr + done;
        size_t pageRemain = AT24_PAGE_SIZE - (cur % AT24_PAGE_SIZE);
        size_t n = len - done;
        if (n > AT24_WRITE_CHUNK)
        {
            n = AT24_WRITE_CHUNK;
        }
        if (n > pageRemain)
        {
            n = pageRemain;
        }

        Wire.beginTransmission(AT24_I2C_ADDR);
        Wire.write((uint8_t)(cur >> 8));
        Wire.write((uint8_t)(cur & 0xFF));
        Wire.write(buf + done, n);
        if (Wire.endTransmission() != 0)
        {
            return false;
        }
        if (!at24WaitReady())
        {
            return false;
        }
        done += n;
    }
    return true;
}
