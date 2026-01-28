#include "Panel.h"

Panel::Panel(uint8_t brightness, bool doubleBuffered)
{
    this->panelBrightness = brightness;
    this->doubleBuffered = doubleBuffered;

    // Module configuration
    // Set refreshRate to '1' to get all colour depths displayed with correct BCM time weighting.
    HUB75_I2S_CFG mxconfig(
        MAT_WIDTH,
        MAT_HEIGHT,
        MAT_CHAIN,
        {(MAT_R1), (MAT_G1), (MAT_B1),                // GPIO Mapping
         (MAT_R2), (MAT_G2), (MAT_B2),                //
         (MAT_A), (MAT_B), (MAT_C), (MAT_D), (MAT_E), //
         (MAT_LAT), (MAT_OE), (MAT_CLK)},             //
        HUB75_I2S_CFG::SHIFTREG,                      // Driver type
        HUB75_I2S_CFG::TYPE138,                       // Line driver type
        doubleBuffered,                               // Double buffer
        HUB75_I2S_CFG::HZ_20M,                        // I2S clock
        DEFAULT_LAT_BLANKING,                         // Latch blanking
        true,                                         // Clock phase
        60,                                           // Minimum refresh rate
        PIXEL_COLOR_DEPTH_BITS_DEFAULT                // Color depth bits
    );
    // Display Setup
    matPanel = new MatrixPanel_I2S_DMA(mxconfig);
    if (!matPanel->begin())
    {
        Logger::println("Panel - MatrixPanel_I2S_DMA::begin() failed");
        return;
    }
    matPanel->setFont(font);

    // initialise clear screen
    matPanel->setBrightness8(panelBrightness);
    matPanel->clearScreen();

    // convenience colors for testing
    myBLACK = matPanel->color565(0, 0, 0);
    myWHITE = matPanel->color565(255, 255, 255);
    myRED = matPanel->color565(255, 0, 0);
    myGREEN = matPanel->color565(0, 255, 0);
    myBLUE = matPanel->color565(0, 0, 255);
}

// clear to black screen
void Panel::clearScreen()
{
    matPanel->clearScreen();
}

// fill the screen with the current HSV color
void Panel::fillScreenHSV(uint16_t hue, uint8_t sat, uint8_t val)
{
    uint16_t color565 = hsvTo565(hue, sat, val);
    matPanel->fillScreen(color565);
}

// set brightness 0-255 of the whole screen
void Panel::setBrightness(uint8_t brightness)
{
    panelBrightness = brightness;
    matPanel->setBrightness8(panelBrightness);
}

//get brightness 0-255 of the whole screen
uint8_t Panel::getBrightness()
{
    return panelBrightness;
}

// draw a pixel to the panel at (x,y) with RGB color
void Panel::drawPixelRGB(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    matPanel->drawPixelRGB888(x, y, r, g, b);
}

// draw a pixel to the panel at (x,y) with 565 color
void Panel::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    matPanel->drawPixel(x, y, color);
}

// write a full buffer to the panel with 565 color
void Panel::writeBuffer(uint16_t buffer[64][32])
{
    for (int x = 0; x < 64; x++)
    {
        for (int y = 0; y < 32; y++)
        {
            matPanel->drawPixel(x, y, buffer[x][y]);
        }
    }
}

// print text to panel at (x,y) with 565 color
void Panel::printText(char *text, int8_t x, int8_t y, uint16_t color)
{
    matPanel->setFont(this->font);
    matPanel->setTextSize(1);
    matPanel->setCursor(x, y);
    matPanel->setTextColor(color);
    matPanel->print(text);
    // log all data to help debugging
   // Logger::printf("Printing text '%s' at (%d,%d) with color 0x%04X\n", text, x, y, color);
}

// return the width in pixels of the given text string, with the current font
int Panel::getTextWidth(String textString)
{
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    matPanel->getTextBounds(textString, 0, 0, &x1, &y1, &textWidth, &textHeight);
    return textWidth;
}

// return the height in pixels of the given text string, with the current font
int Panel::getTextHeight(String textString)
{
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    matPanel->getTextBounds(textString, 0, 0, &x1, &y1, &textWidth, &textHeight);
    return textHeight;
}

// set font for text drawing
void Panel::setFont(const GFXfont *font)
{
    this->font = font;
}

// set font color for text drawing
void Panel::setFontColor(uint16_t color)
{
    this->fontColor = color;
}

// for double buffering, swap the DMA buffers
void Panel::swapDMABuffers()
{
    matPanel->flipDMABuffer();
}

// convert 24-bit RGB to 16-bit RGB565
uint16_t Panel::rgbTo565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
}

// convert HSV to 565 ( hue : 0-65535,  sat : 0-255,  val : 0-255)
uint16_t Panel::hsvTo565(uint16_t hue, uint8_t sat, uint8_t val)
{
    uint32_t hsvColor = Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue, sat, val));
    uint8_t r = (hsvColor >> 16) & 0xFF;
    uint8_t g = (hsvColor >> 8) & 0xFF;
    uint8_t b = hsvColor & 0xFF;
    return rgbTo565(r, g, b);
}

Panel::~Panel()
{
}