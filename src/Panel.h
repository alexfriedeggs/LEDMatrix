#ifndef PANEL_H
#define PANEL_H

#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "Logger.h"

#include <Fonts/FreeMono9pt7b.h>

#define MAT_WIDTH 64  // pixels wide
#define MAT_HEIGHT 32 // pixels high
#define MAT_CHAIN 1   // number of panels chained

// HUB75 pin-mapping
#define MAT_A 10   // A row selection
#define MAT_B 6    // B line selection
#define MAT_C 18   // C row selection
#define MAT_D 7    // D line selection
#define MAT_E -1   // E row selection   // Not used in 32-row panels
#define MAT_R1 14  // High-order R data
#define MAT_R2 12  // Low-order R data
#define MAT_G1 4   // High-order G data
#define MAT_G2 5   // Low-order G data
#define MAT_B1 13  // High-order B data
#define MAT_B2 11  // Low-order B data
#define MAT_CLK 17 // Matrix Clk input
#define MAT_LAT 15 // Matrix latch pin
#define MAT_OE 16  // Matrix Output Enable

class Panel
{
public:
    // constructor. brightness: 0-255,
    Panel(uint8_t brightness = 200, bool doubleBuffered = true);
    ~Panel();

    void clearScreen();
    void fillScreenHSV(uint16_t hue, uint8_t sat, uint8_t val);
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness();

    // draw a pixel to the panel at (x,y) with RGB color
    void drawPixelRGB(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    // draw a pixel to the panel at (x,y) with 565 color
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    // write a full buffer to the panel with 565 color
    void writeBuffer(uint16_t buffer[MAT_WIDTH][MAT_HEIGHT]);

    // set font for text drawing
    void setFont(const GFXfont *font);
    // set font color for text drawing
    void setFontColor(uint16_t color);

    // print text to panel at (x,y) with 565 color
    void printText(char *text, int8_t x, int8_t y, uint16_t color);
    int getTextWidth(String textString);
    int getTextHeight(String textString);

    // for double buffering, swap the DMA buffers
    void swapDMABuffers();
    bool isDoubleBuffered() { return doubleBuffered; }
    int getCalculatedRefreshRate() { return matPanel->calculated_refresh_rate; }

    // convert 24-bit RGB to 16-bit RGB565
    uint16_t rgbTo565(uint8_t r, uint8_t g, uint8_t b);
    // convert HSV to 565 ( hue : 0-65535,  sat : 0-255,  val : 0-255)
    uint16_t hsvTo565(uint16_t hue, uint8_t sat, uint8_t val);

private:
    MatrixPanel_I2S_DMA *matPanel = nullptr;
    uint8_t panelBrightness = 200; // 0-255
    bool doubleBuffered;

    const GFXfont *font = &FreeMono9pt7b;
    uint16_t fontColor = 0xFFFF;

    // TESTING
    uint16_t myBLACK, myWHITE, myRED, myGREEN, myBLUE;
};

#endif