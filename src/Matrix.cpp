#include "Matrix.h"

// convert 24-bit RGB to 16-bit RGB565
uint16_t Matrix::rgbTo565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
}

// convert HSV to 565 ( hue : 0-65535,  sat : 0-255,  val : 0-255)
uint16_t Matrix::hsvTo565(uint16_t hue, uint8_t sat, uint8_t val)
{
    uint32_t hsvColor = Adafruit_NeoPixel::Adafruit_NeoPixel::ColorHSV(hue, sat, val);
  //  uint32_t hsvColor = Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue, sat, val));
    uint8_t r = (hsvColor >> 16) & 0xFF;
    uint8_t g = (hsvColor >> 8) & 0xFF;
    uint8_t b = hsvColor & 0xFF;
    return rgbTo565(r, g, b);
}