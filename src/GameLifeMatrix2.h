#ifndef GAMELIFEMATRIX2_H
#define GAMELIFEMATRIX2_H

#pragma once

#include <FastLED.h>
#include <atomic>
#include "Matrix.h"

#define BACKGROUND_MODE_RELATIVE_BRIGHTNESS_GAME 0.9f
#define FOREGROUND_MODE_RELATIVE_BRIGHTNESS_GAME 1.0f
#define UNDERPOPULATION_DEATH_CHANCE 99
#define OVERPOPULATION_DEATH_CHANCE 95
// 1. Any live cell with less than two live neighbours has UNDERPOPULATION_DEATH_CHANCE%
// chance of dying due to underpopulation.
// 2. Any live cell with two or three live neighbours lives on to the next generation.
// 3. Any live cell with more than three live neighbours has OVERPOPULATION_DEATH_CHANCE%
// chance of dying due to overpopulation.
// 4. Any dead cell with exactly three or 6 live neighbours becomes a live cell by
// reproduction.
// 5. Otherwise the cell remains dead.

class GameLifeMatrix2 : public Matrix
{
public:
    GameLifeMatrix2(int initDensityPercentage = 45, bool edgeWrap = true);
    ~GameLifeMatrix2();
    void initialise() override;
    void calcNewStates() override;

    // move to next pallette in list
    void nextPalette() override
    {
        int index = currentPaletteIndex.load();
        index = (index + 1) % (sizeof(palettes) / sizeof(palettes[0]));
        currentPaletteIndex.store(index);
        Logger::printf("Switched to palette index %d\n", index);
    }

private:
    bool edgeWrap = true;
    int initDensityPercentage = 45; // percentage chance of a cell being alive at start

    // 2D buffers to hold boolean alive/dead cell states
    std::array<std::array<bool, MATRIX_ARRAY_HEIGHT>, MATRIX_ARRAY_WIDTH> bufferBoolPrimary;
    std::array<std::array<bool, MATRIX_ARRAY_HEIGHT>, MATRIX_ARRAY_WIDTH> bufferBoolSecondary;

    // color palettes
    std::atomic<int> currentPaletteIndex;
    CRGBPalette16 palettes[8] = {HeatColors_p, LavaColors_p, ForestColors_p, CloudColors_p, OceanColors_p,
                                 PartyColors_p, RainbowColors_p, RainbowStripeColors_p};
    CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND)
    {
        return ColorFromPalette(palettes[currentPaletteIndex.load()], index, brightness, blendType);
    }

    // relative brightnesses of each state (0-1.0f, multiplied by palette color brightness)
    float aliveBrightness = 1.0f;
    float justBornBrightness = 1.0f;
    float justDiedBrightness = 0.7f;
    float deadBrightness = 0.3f;

    // Current frame palette indices
    int alivePalInd; // changes each frame
    int justBornPalIndOffset = 20;
    int justDiedPalIndOffset = -20;
    int deadPalIndOffset = 128;

    // Current frame colors
    CRGB aliveRGB{0, 0, 0};
    CRGB justBornRGB{0, 0, 0};
    CRGB justDiedRGB{0, 0, 0};
    CRGB deadRGB{0, 0, 0};

    // influence of previous cell color on new color (0-255)
    // 0= no influence, 255 = full influence
    uint8_t prevCellInfluence = 200;

    // update the colors from the current indices into palette
    void calcFrameColors();

    // get the number of live neighbors for cell at (x,y) (8 neighbours)
    int getLiveNeighborCount(int x, int y);

    // run Conway's Game of Life rules with some randomness
    bool getNewState(int x, int y, int liveNeighbors);

    // get the new color value for this cell based on previous state and current state
    uint16_t getNewColorValue(bool currentState, bool prevState, uint16_t prevColor);
};

#endif