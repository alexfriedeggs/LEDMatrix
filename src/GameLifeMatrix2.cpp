#include "GameLifeMatrix2.h"

GameLifeMatrix2::GameLifeMatrix2(int initDensityPercentage, bool edgeWrap)
{
    this->backgroundModeRelativeBrightness = BACKGROUND_MODE_RELATIVE_BRIGHTNESS_GAME;
    this->foregroundModeRelativeBrightness = FOREGROUND_MODE_RELATIVE_BRIGHTNESS_GAME;

    this->initDensityPercentage = initDensityPercentage;
    this->edgeWrap = edgeWrap;

    // set dafaults
    backgroundModeRelativeBrightness = 0.621f;
    this->backgroundMode.store(true);
    this->currentRelativeBrightness.store(this->backgroundModeRelativeBrightness);

    // Set starting FastLED palette
    currentPaletteIndex.store(0);
    alivePalInd = 0;

    initialise();
}

// Initialize the current state buffer with random values
void GameLifeMatrix2::initialise()
{
    // uses internal heat-based random generator
    randomSeed(esp_random());

    // calc the alive/dead/justBorn/justDied colors based on palette,index and brightness settings
    calcFrameColors();

    // initial alive/dead colors
    uint16_t aliveCol = rgbTo565(aliveRGB.r, aliveRGB.g, aliveRGB.b);
    uint16_t deadCol = rgbTo565(deadRGB.r, deadRGB.g, deadRGB.b);
    alivePalInd = random(0, 255);   // start with random palette index

    uint16_t color565;
    for (int x = 0; x < MATRIX_ARRAY_WIDTH; x++)
    {
        for (int y = 0; y < MATRIX_ARRAY_HEIGHT; y++)
        {
            // boolean alive/dead states
            bufferBoolPrimary[x][y] = (random(0, 100) < initDensityPercentage) ? true : false;
            bufferBoolSecondary[x][y] = false;
            // colors based on alive/dead states
            bufferPrimary[x][y] = bufferBoolPrimary[x][y] ? aliveCol : deadCol;
            bufferSecondary[x][y] = deadCol;
        }
    }
}

// Calculate new states based on current states
void GameLifeMatrix2::calcNewStates()
{
    for (int x = 0; x < MATRIX_ARRAY_WIDTH; x++)
    {
        for (int y = 0; y < MATRIX_ARRAY_HEIGHT; y++)
        {
            // get number of live neighbors (alive or just born) for each cell
            int liveNeighbors = getLiveNeighborCount(x, y);

            // Determine new boolean state based on Game of Life rules
            bufferBoolSecondary[x][y] = getNewState(x, y, liveNeighbors);

            // determine color based on new state and previous state and previous color
            bufferSecondary[x][y] = getNewColorValue(bufferBoolSecondary[x][y], bufferBoolPrimary[x][y],
                                                     bufferPrimary[x][y]);
        }
    }

    // now swap buffers. this puts the new cell states into the primary buffer
    // and stores the old states in the secondary buffer. Note that the
    // old states are overwritten on the next calcNewStates() call.
    std::swap(bufferBoolPrimary, bufferBoolSecondary);
    std::swap(bufferPrimary, bufferSecondary);

    if (cycling.load())
        alivePalInd += 1; // increment palette index for next frame if needed

    // Update colors based on current palette and index & store rgbs for next frame
    calcFrameColors();
}

int GameLifeMatrix2::getLiveNeighborCount(int x, int y)
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

bool GameLifeMatrix2::getNewState(int x, int y, int liveNeighbors)
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
uint16_t GameLifeMatrix2::getNewColorValue(bool currentState, bool prevState, uint16_t prevColor)
{
    CRGB baseColorRGB;

    if (currentState)
    {
        if (prevState) // cell is alive and was alive
        {
            baseColorRGB = aliveRGB;
        }
        else // cell is alive but was dead (just born)
        {
            baseColorRGB = justBornRGB;
        }
    }
    else
    {
        if (prevState) // cell is dead but was alive (just died)
        {
            baseColorRGB = justDiedRGB;
        }
        else // cell is dead and was dead
        {
            baseColorRGB = deadRGB;
        }
    }

    // now blend with previous cell color based on influence factor
    if (prevCellInfluence > 0)
    {
        uint8_t rPrev, gPrev, bPrev;
        // extract r, g, b from 565 color and convert to 8-bit
        getRGBFrom565(prevColor, rPrev, gPrev, bPrev);
        // blend new color with previous color based on influence factor
        uint8_t rNew = (baseColorRGB.r * (255 - prevCellInfluence) + rPrev * prevCellInfluence) >> 8; // >> 8 is divide by 256
        uint8_t gNew = (baseColorRGB.g * (255 - prevCellInfluence) + gPrev * prevCellInfluence) >> 8;
        uint8_t bNew = (baseColorRGB.b * (255 - prevCellInfluence) + bPrev * prevCellInfluence) >> 8;

        // static uint32_t callCount = 0;
        // callCount++;
        // if (callCount > 15000) // log every 15000 calls for debugging
        // {
        //     callCount = 0;
        //     Logger::printf("Palette Index: %d\n", alivePalInd);
        //     Logger::printf("Base Color RGB: R:%d G:%d B:%d\n", baseColorRGB.r, baseColorRGB.g, baseColorRGB.b);
        //     Logger::printf("Prev Color RGB: R:%d G:%d B:%d\n", rPrev, gPrev, bPrev);
        //     Logger::printf("New Color RGB: R:%d G:%d B:%d\n", rNew, gNew, bNew);
        // }

        // convert blended color back to 565 and return
        return rgbTo565(rNew, gNew, bNew);
    }

    // no influence, just return base color
    return rgbTo565(baseColorRGB.r, baseColorRGB.g, baseColorRGB.b);
}

// update the colors from the current palette and indices once per frame
void GameLifeMatrix2::calcFrameColors()
{
    float relativeBrightness = currentRelativeBrightness.load();

    uint8_t adjustedAliveBrightness = (uint8_t) (255 * relativeBrightness * aliveBrightness);
    uint8_t adjustedJustBornBrightness = (uint8_t) (255 * relativeBrightness * justBornBrightness);
    uint8_t adjustedJustDiedBrightness = (uint8_t) (255 * relativeBrightness * justDiedBrightness);
    uint8_t adjustedDeadBrightness = (uint8_t) (255 * relativeBrightness * deadBrightness);

    aliveRGB = ColorFromCurrentPalette(alivePalInd, adjustedAliveBrightness);
    justDiedRGB = ColorFromCurrentPalette(justDiedPalIndOffset + alivePalInd, adjustedJustDiedBrightness);
    justBornRGB = ColorFromCurrentPalette(justBornPalIndOffset + alivePalInd, adjustedJustBornBrightness);
    deadRGB = ColorFromCurrentPalette(deadPalIndOffset + alivePalInd, adjustedDeadBrightness);

    // every 50 frames log the color values for debugging
    static int frameCount = 0;
    frameCount++;
    if (frameCount >= 50)
    {        frameCount = 0;
        Logger::printf("Current palette Index: %d\n", currentPaletteIndex.load());
        Logger::printf("Alive Index in palette: %d\n", alivePalInd);
        Logger::printf("Relative Brightness: %.2f\n", relativeBrightness);
        Logger::printf("Adjusted Brightnesses - Alive: %d, JustBorn: %d, JustDied: %d, Dead: %d\n",
                       adjustedAliveBrightness, adjustedJustBornBrightness, adjustedJustDiedBrightness, adjustedDeadBrightness);
        Logger::printf("RGB colors: Alive R:%d G:%d B:%d | JustBorn R:%d G:%d B:%d | JustDied R:%d G:%d B:%d | Dead R:%d G:%d B:%d\n",
                       aliveRGB[0], aliveRGB[1], aliveRGB[2],
                       justBornRGB[0], justBornRGB[1], justBornRGB[2],
                       justDiedRGB[0], justDiedRGB[1], justDiedRGB[2],
                       deadRGB[0], deadRGB[1], deadRGB[2]);
    }   

    // // every 10 frames log the color values for debugging
    // static int frameCount = 0;
    // frameCount++;
    // if (frameCount >= 10)
    // {
    //     frameCount = 0;

    //     Logger::printf("Original HSV: Hue: %d, Sat: %d, Val: %d\n", hsvHue, hsvSat, hsvVal);
    //     Logger::printf("Relative Brightness: %.2f\n", relativeBrightness);
    //     Logger::printf("Adjusted Vals - Alive: %d, JustBorn: %d, JustDied: %d, Dead: %d\n",
    //                    adjustedAliveVal, adjustedJustBornVal, adjustedJustDiedVal, adjustedDeadVal);
    //     Logger::printf("RGB colors: Alive R:%d G:%d B:%d | JustBorn R:%d G:%d B:%d | JustDied R:%d G:%d B:%d | Dead R:%d G:%d B:%d\n",
    //                    aliveRGB[0], aliveRGB[1], aliveRGB[2],
    //                    justBornRGB[0], justBornRGB[1], justBornRGB[2],
    //                    justDiedRGB[0], justDiedRGB[1], justDiedRGB[2],
    //                    deadRGB[0], deadRGB[1], deadRGB[2]);
    // }
}

GameLifeMatrix2::~GameLifeMatrix2()
{
}