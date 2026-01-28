#ifndef MATRIX_H
#define MATRIX_H

#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Logger.h"

// Abstract base class for matrix-based algorithms
class Matrix
{
public:
    static const int MATRIX_ARRAY_WIDTH = 64;
    static const int MATRIX_ARRAY_HEIGHT = 32;

    Matrix() {}          // Inline empty constructor
    virtual ~Matrix() {} // Inline empty destructor

    // pure virtual functions to be implemented by derived classes
    virtual void initialise() = 0;    // initialize the matrix states
    virtual void calcNewStates() = 0; // calculate new states

    // common interface to get cell colors
    uint16_t getCellColor(int x, int y)
    {
        if (x < 0 || x >= MATRIX_ARRAY_WIDTH || y < 0 || y >= MATRIX_ARRAY_HEIGHT)
            return 0x0000; // out of bounds
        return bufferPrimary[x][y];
    }
    uint16_t getPrevCellColor(int x, int y)
    {
        if (x < 0 || x >= MATRIX_ARRAY_WIDTH || y < 0 || y >= MATRIX_ARRAY_HEIGHT)
            return 0x0000; // out of bounds
        return bufferSecondary[x][y];
    }

    // set whether drawing in background mode (lower brightness) or foreground mode (higher brightness)
    void setBackgroundMode(bool backgroundMode)
    {
        this->backgroundMode.store(backgroundMode);
        if (backgroundMode)
        {
            currentRelativeBrightness.store(backgroundModeRelativeBrightness);
        }
        else
        {
            currentRelativeBrightness.store(foregroundModeRelativeBrightness);
        }
    }

    // helper functions available publicly:

    // convert 24-bit RGB to 16-bit RGB565
    uint16_t rgbTo565(uint8_t r, uint8_t g, uint8_t b);
    // convert HSV to 565 ( hue : 0-65535,  sat : 0-255,  val : 0-255)
    uint16_t hsvTo565(uint16_t hue, uint8_t sat, uint8_t val);

protected:
    // 2D buffers to hold cell states
    std::array<std::array<uint16_t, MATRIX_ARRAY_HEIGHT>, MATRIX_ARRAY_WIDTH> bufferPrimary;
    std::array<std::array<uint16_t, MATRIX_ARRAY_HEIGHT>, MATRIX_ARRAY_WIDTH> bufferSecondary;

    std::atomic<bool> backgroundMode; // drawing in background or foreground
    std::atomic<float> currentRelativeBrightness;        // current brightness factor based on mode

    // reltive brightness factors: set these in child classes as needed
    // e.g. 0.3f : background is 30% of the brightness of foreground
    float backgroundModeRelativeBrightness = 0.5f; // (0-1.0f) brightness factor for background mode
    float foregroundModeRelativeBrightness = 1.0f; // (0-1.0f) brightness factor for foreground mode
};

#endif