#ifndef PLASMAMATRIX_H
#define PLASMAMATRIX_H

#pragma once

#include <FastLED.h>
#include "Matrix.h"

#define BACKGROUND_MODE_RELATIVE_BRIGHTNESS 0.6f
#define FOREGROUND_MODE_RELATIVE_BRIGHTNESS 1.0f

class PlasmaMatrix: public Matrix
{
public:
    PlasmaMatrix();
    ~PlasmaMatrix();

    void initialise() override;
    void calcNewStates() override;

private:
    CRGB currentColor;
    CRGBPalette16 palettes[5] = {HeatColors_p, LavaColors_p, RainbowColors_p,
                                 RainbowStripeColors_p, CloudColors_p};
    CRGBPalette16 currentPalette = palettes[0];

    uint16_t time_counter = 0;
    uint16_t cycles = 0;

    CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND)
    {
        return ColorFromPalette(currentPalette, index, brightness, blendType);
    }
};

// end loop

#endif