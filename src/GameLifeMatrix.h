#ifndef GAMELIFEMATRIX_H
#define GAMELIFEMATRIX_H

#pragma once

#include "Matrix.h"

#define BACKGROUND_MODE_RELATIVE_BRIGHTNESS 0.6f
#define FOREGROUND_MODE_RELATIVE_BRIGHTNESS 1.0f
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

class GameLifeMatrix : public Matrix
{
public:
    GameLifeMatrix(int initDensityPercentage = 45, bool edgeWrap = true);
    ~GameLifeMatrix();
    void initialise() override;
    void calcNewStates() override;

private:
    bool edgeWrap = true;
    int initDensityPercentage = 45; // percentage chance of a cell being alive at start

    // 2D buffers to hold boolean alive/dead cell states
    std::array<std::array<bool, MATRIX_ARRAY_HEIGHT>, MATRIX_ARRAY_WIDTH> bufferBoolPrimary;
    std::array<std::array<bool, MATRIX_ARRAY_HEIGHT>, MATRIX_ARRAY_WIDTH> bufferBoolSecondary;

    // Current frame colors
    uint16_t aliveCol = 0;
    uint16_t justBornCol = 0;
    uint16_t justDiedCol = 0;
    uint16_t deadCol = 0;
    uint8_t aliveRGB[3] = {0, 0, 0}; 
    uint8_t justBornRGB[3] = {0, 0, 0};
    uint8_t justDiedRGB[3] = {0, 0, 0};
    uint8_t deadRGB[3] = {0, 0, 0};

    // HSV values for color generation
    uint16_t hsvHue = 0;
    uint8_t hsvSat = 220;
    uint8_t hsvVal = 225;
    uint8_t hsvValJustDied = (hsvVal / 3) * 2;
    uint8_t hsvValJustBorn = 255;
    uint8_t hsvValDead = 125;

    // influence of previous cell color on new color (0-255)
    // 0= no influence, 255 = full influence
    uint8_t prevCellInfluence = 20; 

    // update the colors from the current HSV values
    void updateColorsFromHSV();

    // get the number of live neighbors for cell at (x,y) (8 neighbours)
    int getLiveNeighborCount(int x, int y);

    // run Conway's Game of Life rules with some randomness
    bool getNewState(int x, int y, int liveNeighbors);

    // get the new color value for this cell based on previous state and current state
    uint16_t getNewColorValue(bool currentState, bool prevState, uint16_t prevColor);

    // extract 8-bit r, g, b from 16-bit 565 color
    void getRGBFrom565(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b);
};

#endif