#include "hal/hal_display.h"

#if LGS_ENABLE_DISPLAY

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_I2C_ADDR 0x3C

static Adafruit_SSD1306 oled(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);
static bool displayReady = false;
static bool displayOn = true;

void displayInit()
{
    if (!oled.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR))
    {
        LOG_WARNING_SYS(F("[DISPLAY] SSD1306 not found\n"));
        return;
    }
    displayReady = true;
    oled.setTextColor(WHITE);
    oled.setTextSize(2);
    oled.setRotation(0);
    oled.clearDisplay();
    oled.display();
    LOG_DEBUG_SYS(F("[DISPLAY] OLED HAL initialized\n"));
}

void displayEnable(bool enabled)
{
    displayOn = enabled;
    if (!displayReady) return;
    if (!enabled) displayClear();
}

void displayShowNumber(uint16_t value)
{
    if (!displayReady || !displayOn) return;
    oled.clearDisplay();
    oled.setCursor(18, 22);
    oled.print(value);
    oled.display();
}

void displayClear()
{
    if (!displayReady) return;
    oled.clearDisplay();
    oled.display();
}

#else // LGS_ENABLE_DISPLAY == 0  -> no-op stubs

void displayInit() {}
void displayEnable(bool) {}
void displayShowNumber(uint16_t) {}
void displayClear() {}

#endif // LGS_ENABLE_DISPLAY
