#include "hw/oled.h"

// Dedicated I2C2 bus for the OLED (separate from the internal I2C1 sensor bus)
TwoWire WireOLED(HW_I2C2_SDA_PIN, HW_I2C2_SCL_PIN);
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &WireOLED, -1);

bool oledInit()
{
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
    oled.setTextSize(textSize);
    oled.setCursor(0, 0);
    oled.println(text);
    oled.display();
}
