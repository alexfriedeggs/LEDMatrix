#include "PlasmaMatrix.h"

PlasmaMatrix::PlasmaMatrix()
{
    this->backgroundModeRelativeBrightness = BACKGROUND_MODE_RELATIVE_BRIGHTNESS;
    this->foregroundModeRelativeBrightness = FOREGROUND_MODE_RELATIVE_BRIGHTNESS;
    // set dafaults
    this->backgroundMode.store(true);
    this->currentRelativeBrightness.store(this->backgroundModeRelativeBrightness);

    initialise();
}

// Initialize the current state buffer with random values
void PlasmaMatrix::initialise()
{
    // Set current FastLED palette
    currentPalette = RainbowColors_p;
}

void PlasmaMatrix::calcNewStates()
{
    uint8_t scaledBrightness = static_cast<uint8_t>(currentRelativeBrightness.load() * 255);

    for (int x = 0; x < MATRIX_ARRAY_WIDTH; x++)
    {
        for (int y = 0; y < MATRIX_ARRAY_HEIGHT; y++)
        {
            int16_t v = 128;
            uint8_t wibble = sin8(time_counter);
            v += sin16(x * wibble * 3 + time_counter);
            v += cos16(y * (128 - wibble) + time_counter);
            v += sin16(y * x * cos8(-time_counter) / 8);

            currentColor = ColorFromPalette(currentPalette, (v >> 8), scaledBrightness); // currentBlendType);
            bufferPrimary[x][y] = rgbTo565(currentColor.r, currentColor.g, currentColor.b);
        }
    }

    ++time_counter;
    ++cycles;

    if (cycles >= 1024)
    {
        time_counter = 0;
        cycles = 0;
        currentPalette = palettes[random(0, sizeof(palettes) / sizeof(palettes[0]))];
    }

}

PlasmaMatrix::~PlasmaMatrix()
{
}