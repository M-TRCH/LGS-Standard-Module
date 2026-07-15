#include "drivers/oled.h"
#include "drivers/oled_font_bignum.h"

// Dedicated I2C2 bus for the OLED (separate from the internal I2C1 sensor bus)
static TwoWire WireOLED(HW_I2C2_SDA_PIN, HW_I2C2_SCL_PIN);
static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &WireOLED, -1);

namespace {
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
        return false;
    }

    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR))
    {
        return false;
    }

    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setRotation(0);
    oled.clearDisplay();
    oled.display();

    return true;
}

void oledClear()
{
    oled.clearDisplay();
    oled.display();
}

void oledPrint(const char *text, uint8_t textSize)
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
    char buf[4];
    sniprintf(buf, sizeof(buf), "%02u", (unsigned)(value % 100)); // two tabular digits

    oled.clearDisplay();
    oled.setFont(&OledBigNum);
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);

    // Centre the two big digits both axes via the font metrics.
    int16_t bx, by;
    uint16_t bw, bh;
    oled.getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
    int16_t x = ((int16_t)OLED_WIDTH - (int16_t)bw) / 2 - bx;
    int16_t y = ((int16_t)OLED_HEIGHT - (int16_t)bh) / 2 - by;
    oled.setCursor(x, y);
    oled.print(buf);

    oled.setFont(nullptr);   // restore the built-in font for other draw calls
    oled.display();
}

namespace {
// Draw a null-terminated string horizontally centered at the given top y,
// using whatever text size is currently set.
void drawCentered(const char *text, int16_t y)
{
    int16_t bx, by;
    uint16_t bw, bh;
    oled.getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
    int16_t x = ((int16_t)OLED_WIDTH - (int16_t)bw) / 2;
    if (x < 0)
    {
        x = 0;
    }
    oled.setCursor(x, y);
    oled.print(text);
}
} // namespace

void oledPrintTitledNumber(const char *title, uint16_t value)
{
    char buf[6];
    // sniprintf = integer-only variant: avoids linking the float-capable
    // vfprintf path (dtoa + double soft-float) that plain snprintf drags in.
    sniprintf(buf, sizeof(buf), "%u", value);

    oled.clearDisplay();
    oled.setFont(nullptr);
    oled.setTextColor(SSD1306_WHITE);

    oled.setTextSize(1);
    drawCentered(title, 4);      // small caption near the top (8px tall)

    oled.setTextSize(4);
    drawCentered(buf, 26);       // large value below (32px tall, fits 0-63)

    oled.display();
}

void oledPrintCentered2(const char *line1, const char *line2, uint8_t textSize)
{
    oled.clearDisplay();
    oled.setFont(nullptr);
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(textSize);

    const int16_t lineHeight = 8 * (int16_t)textSize;
    const int16_t lineGap = 2;
    bool twoLines = (line2 != nullptr && line2[0] != '\0');

    int16_t blockHeight = twoLines ? (lineHeight * 2 + lineGap) : lineHeight;
    int16_t top = ((int16_t)OLED_HEIGHT - blockHeight) / 2;
    if (top < 0)
    {
        top = 0;
    }

    drawCentered(line1, top);
    if (twoLines)
    {
        drawCentered(line2, top + lineHeight + lineGap);
    }

    oled.display();
}
