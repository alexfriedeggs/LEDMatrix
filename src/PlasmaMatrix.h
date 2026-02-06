#ifndef PLASMAMATRIX_H
#define PLASMAMATRIX_H

#pragma once

#include <FastLED.h>
#include "Matrix.h"

#define BACKGROUND_MODE_RELATIVE_BRIGHTNESS_PLASMA 0.6f
#define FOREGROUND_MODE_RELATIVE_BRIGHTNESS_PLASMA 1.0f

class PlasmaMatrix : public Matrix
{
public:
    PlasmaMatrix();
    ~PlasmaMatrix();

    void initialise() override;
    void calcNewStates() override;

    // move to next pallette in list
    void nextPalette() override
    {
        int index = currentPaletteIndex.load();
        index = (index + 1) % (sizeof(palettes) / sizeof(palettes[0]));
        currentPalette = palettes[index];
        currentPaletteIndex.store(index);
        Logger::printf("Switched to palette index %d\n", index);
    }

private:
    CRGB currentColor;
    CRGBPalette16 palettes[8] = {HeatColors_p, LavaColors_p, ForestColors_p, CloudColors_p, OceanColors_p,
                                 PartyColors_p, RainbowColors_p, RainbowStripeColors_p};
    CRGBPalette16 currentPalette;
    std::atomic<int> currentPaletteIndex;

    uint16_t time_counter = 0;
    uint16_t cycles = 0;

    CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND)
    {
        return ColorFromPalette(currentPalette, index, brightness, blendType);
    }
};

// end loop

#endif