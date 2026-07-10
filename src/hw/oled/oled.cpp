#include "hw/oled.h"
#include "hw/oled_font_digits.h"

// Dedicated I2C2 bus for the OLED (separate from the internal I2C1 sensor bus)
TwoWire WireOLED(HW_I2C2_SDA_PIN, HW_I2C2_SCL_PIN);
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &WireOLED, -1);

namespace {
constexpr int DIGIT_GAP = 0;   // spacing between the two tabular digits

static bool oledProbeAddress(uint8_t address)
{
    WireOLED.beginTransmission(address);
    return (WireOLED.endTransmission() == 0);
}
}

bool oledInit()
{
    WireOLED.begin();

    if (!oledProbeAddress(OLED_I2C_ADDR))
    {
        LOG_INFO_SYS(F("[OLED] No device found on I2C2 at 0x3C, skipping init\n"));
        return false;
    }

    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR))
    {
        LOG_WARNING_SYS(F("[OLED] SSD1306 init failed\n"));
        return false;
    }

    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setRotation(0);
    oled.clearDisplay();
    oled.display();

    LOG_INFO_SYS(F("[OLED] Initialized\n"));
    return true;
}

void oledClear()
{
    oled.clearDisplay();
    oled.display();
}

void oledPrint(const String &text, uint8_t textSize)
{
    oled.clearDisplay();
    oled.setFont(nullptr);
    oled.setTextSize(textSize);
    oled.setCursor(0, 0);
    oled.println(text);
    oled.display();
}

void oledPrintLargeNumber(uint8_t value)
{
    uint8_t leftDigit = (value / 10) % 10;
    uint8_t rightDigit = value % 10;
    int16_t totalWidth = (OLED_DIGIT_WIDTH * 2) + DIGIT_GAP;
    int16_t originX = (OLED_WIDTH - totalWidth) / 2;

    oled.clearDisplay();
    oled.setFont(nullptr);
    oled.setTextSize(1);
    oled.drawBitmap(originX, 0, kOledDigits[leftDigit], OLED_DIGIT_WIDTH, OLED_DIGIT_HEIGHT, SSD1306_WHITE);
    oled.drawBitmap(originX + OLED_DIGIT_WIDTH + DIGIT_GAP, 0, kOledDigits[rightDigit], OLED_DIGIT_WIDTH, OLED_DIGIT_HEIGHT, SSD1306_WHITE);
    oled.display();
}
