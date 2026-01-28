#include "GameLifeMatrix.h"

GameLifeMatrix::GameLifeMatrix(int initDensityPercentage, bool edgeWrap)
{
    this->backgroundModeRelativeBrightness = BACKGROUND_MODE_RELATIVE_BRIGHTNESS;
    this->foregroundModeRelativeBrightness = FOREGROUND_MODE_RELATIVE_BRIGHTNESS;

    this->initDensityPercentage = initDensityPercentage;
    this->edgeWrap = edgeWrap;

    // set dafaults
    this->backgroundMode.store(true);
    this->currentRelativeBrightness.store(this->backgroundModeRelativeBrightness);

    initialise();
}

// Initialize the current state buffer with random values
void GameLifeMatrix::initialise()
{
    // uses internal heat-based random generator
    randomSeed(esp_random());

    // Initialize colors
    updateColorsFromHSV();

    for (int x = 0; x < MATRIX_ARRAY_WIDTH; x++)
    {
        for (int y = 0; y < MATRIX_ARRAY_HEIGHT; y++)
        {
            bufferBoolPrimary[x][y] = (random(0, 100) < initDensityPercentage) ? true : false;
            bufferBoolSecondary[x][y] = false;
            bufferPrimary[x][y] = bufferBoolPrimary[x][y] ? aliveCol : deadCol;
            bufferSecondary[x][y] = deadCol;
        }
    }
}

// Calculate new states based on current states
void GameLifeMatrix::calcNewStates()
{
    // colour base-values for next frame
    hsvHue += 128;
    // Update colors based on current HSV values
    updateColorsFromHSV();

    for (int x = 0; x < MATRIX_ARRAY_WIDTH; x++)
    {
        for (int y = 0; y < MATRIX_ARRAY_HEIGHT; y++)
        {
            // get number of live neighbors (alive or just born) for each cell
            int liveNeighbors = getLiveNeighborCount(x, y);

            // Determine new boolean state based on Game of Life rules
            bool newState = getNewState(x, y, liveNeighbors);

            // determine color based on new state and previous value
            bufferSecondary[x][y] = getNewColorValue(newState, bufferBoolPrimary[x][y], bufferPrimary[x][y]);
        }
    }

    // now swap buffers. this puts the new cell states into the primary buffer
    // and stores the old states in the secondary buffer. Note that the
    // old states are overwritten on the next calcNewStates() call.
    std::swap(bufferBoolPrimary, bufferBoolSecondary);
    std::swap(bufferPrimary, bufferSecondary);
}

int GameLifeMatrix::getLiveNeighborCount(int x, int y)
{
    int liveNeighbors = 0;
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            if (dx == 0 && dy == 0)
                continue; // Skip the cell itself

            int nx = x + dx;
            int ny = y + dy;

            if (edgeWrap)
            {
                nx = (nx + MATRIX_ARRAY_WIDTH) % MATRIX_ARRAY_WIDTH;   // Wrap around edges
                ny = (ny + MATRIX_ARRAY_HEIGHT) % MATRIX_ARRAY_HEIGHT; // Wrap around edges
            }
            else
            {
                if (nx < 0 || nx >= MATRIX_ARRAY_WIDTH || ny < 0 || ny >= MATRIX_ARRAY_HEIGHT)
                    continue; // Out of bounds
            }

            // neighbour is alive
            if (bufferBoolPrimary[nx][ny])
                liveNeighbors++;
        }
    }
    return liveNeighbors;
}

bool GameLifeMatrix::getNewState(int x, int y, int liveNeighbors)
{
    // Apply Conway's Game of Life rules
    bool currentState = bufferBoolPrimary[x][y];

    static int underpopDeathThreshold = UNDERPOPULATION_DEATH_CHANCE * 10; // scale to 0-1000
    static int overpopDeathThreshold = OVERPOPULATION_DEATH_CHANCE * 10;   // scale to 0-1000

    // CURRENTLY LIVE CELL
    //
    if (currentState)
    { // Any live cell with less than two live neighbours dies by underpopulation
        // with very small chance of surviving
        if (liveNeighbors < 2)
        {
            return (random(0, 1000) > underpopDeathThreshold); // 1% chance of survival
        }

        // Any live cell with two or three live neighbours lives on to the next generation.
        else if ((liveNeighbors == 2 || liveNeighbors == 3))
            return true;

        // Any live cell with more than three live neighbours dies by overpopulation.
        // with a very small chance of surviving
        else if (liveNeighbors > 3)
        {
            return (random(0, 1000) > overpopDeathThreshold); // 5% chance of survival
        }
    }

    // CURRENTLY DEAD CELL
    //
    // Any dead cell with exactly three or 6 live neighbours becomes a live cell by reproduction.
    else if ((liveNeighbors == 3 || liveNeighbors == 6))
        return true;

    // otherwise dead cell remains dead
    else
    {
        // a very small chance to become alive
        // return  (!currentState && (random(0, 1000) < 1)) // 0.1% chance
        return false;
    }

    return false;
}

// get the new color value for this cell based on previous state and current state
// as well as new and prev colours
uint16_t GameLifeMatrix::getNewColorValue(bool currentState, bool prevState, uint16_t prevColor)
{
    uint16_t baseColor;
    uint8_t r1, g1, b1;

    if (currentState)
    {
        if (prevState) // cell is alive and was alive
        {
            baseColor = aliveCol;
            r1 = aliveRGB[0];
            g1 = aliveRGB[1];
            b1 = aliveRGB[2];
        }
        else // cell is alive but was dead (just born)
        {
            baseColor = justBornCol;
            r1 = justBornRGB[0];
            g1 = justBornRGB[1];
            b1 = justBornRGB[2];
        }
    }
    else
    {
        if (prevState) // cell is dead but was alive (just died)
        {
            baseColor = justDiedCol;
            r1 = justDiedRGB[0];
            g1 = justDiedRGB[1];
            b1 = justDiedRGB[2];
        }
        else // cell is dead and was dead
        {
            baseColor = deadCol;
            r1 = deadRGB[0];
            g1 = deadRGB[1];
            b1 = deadRGB[2];
        }
    }

    // now blend with previous cell color based on influence factor
    if (prevCellInfluence > 0)
    {
        uint8_t rPrev, gPrev, bPrev;
        getRGBFrom565(prevColor, rPrev, gPrev, bPrev);
        uint8_t rNew = (r1 * (255 - prevCellInfluence) + rPrev * prevCellInfluence) >> 8; // >> 8 is divide by 256
        uint8_t gNew = (g1 * (255 - prevCellInfluence) + gPrev * prevCellInfluence) >> 8;
        uint8_t bNew = (b1 * (255 - prevCellInfluence) + bPrev * prevCellInfluence) >> 8;
        return rgbTo565(rNew, gNew, bNew);
    }

    // no influence, just return base color
    return baseColor;
}

// update the colors from the current HSV values
void GameLifeMatrix::updateColorsFromHSV()
{
    uint8_t adjustedAliveVal = currentRelativeBrightness.load() * hsvVal;
    uint8_t adjustedJustBornVal = currentRelativeBrightness.load() * hsvValJustBorn;
    uint8_t adjustedJustDiedVal = currentRelativeBrightness.load() * hsvValJustDied;
    uint8_t adjustedDeadVal = currentRelativeBrightness.load() * hsvValDead;

    aliveCol = hsvTo565(hsvHue, hsvSat, adjustedAliveVal);
    justDiedCol = hsvTo565((hsvHue + 5000), hsvSat, adjustedJustDiedVal);
    justBornCol = hsvTo565((hsvHue - 5000), hsvSat, adjustedJustBornVal);
    deadCol = hsvTo565((hsvHue + 16384), hsvSat, adjustedDeadVal);

    // also extract RGB values for future use in this frame
    getRGBFrom565(aliveCol, aliveRGB[0], aliveRGB[1], aliveRGB[2]);
    getRGBFrom565(justBornCol, justBornRGB[0], justBornRGB[1], justBornRGB[2]);
    getRGBFrom565(justDiedCol, justDiedRGB[0], justDiedRGB[1], justDiedRGB[2]);
    getRGBFrom565(deadCol, deadRGB[0], deadRGB[1], deadRGB[2]);
}

// extract 8-bit r, g, b from 16-bit 565 color
void GameLifeMatrix::getRGBFrom565(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b)
{
    r = ((color >> 11) & 0x1F) * 8;
    g = ((color >> 5) & 0x3F) * 4;
    b = (color & 0x1F) * 8;
}

GameLifeMatrix::~GameLifeMatrix()
{
}