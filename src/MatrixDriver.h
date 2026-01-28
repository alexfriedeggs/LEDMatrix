#ifndef MATRIXDRIVER_H
#define MATRIXDRIVER_H

#pragma once
#include <atomic>
#include <Arduino.h>

#include "Fonts/FreeMonoBold12pt7b.h"
#define DEFAULT_FONT FreeMonoBold12pt7b

#include "Panel.h"
#include "Matrix.h"
#include "GY21Sensor.h"
#include "Logger.h"
#include "MODES.h"

#define MAX_FPS 120

// class to manage the matrix display updates in a background task
// at a specified frames-per-second rate
class MatrixDriver
{
public:
    // create a MatrixDriver to update the given panel from the given matrix at the given fps
    MatrixDriver(int fps, Panel *panel, Matrix *matrix, GY21Sensor *gy21Sensor,
                 const GFXfont *temperatureFont = nullptr, const GFXfont *humidityFont = nullptr,
                 const uint16_t temperatureFontColor = 0xFFFF, const uint16_t humidityFontColor = 0xFFFF);
    ~MatrixDriver();

    void resume();
    void pause();

    void setMatrix(Matrix *matrix);
    void enableTextDrawing(bool enable);
    void enableBackgroundDrawing(bool enable);

    void setFPS(int fps);
    // set panel brightness 0-255..note this is only applied to panel in the update task
    void setPanelBrightness(uint8_t brightness);

    // Text related functions
    void setTemperatureText(const char *text);
    void setTemperatureTextPosition(uint8_t x, uint8_t y);
    void setTemperatureTextXOffset(int8_t xOffset);
    void setTemperatureTextYOffset(int8_t yOffset);
    void setTemperatureFont(const GFXfont *font);
    void setTemperatureFontColor(uint16_t color);

    void setHumidityText(const char *text);
    void setHumidityTextPosition(uint8_t x, uint8_t y);
    void setHumidityTextXOffset(int8_t xOffset);
    void setHumidityTextYOffset(int8_t yOffset);
    void setHumidityFont(const GFXfont *font);
    void setHumidityFontColor(uint16_t color);

private:
    Panel *panel;
    GY21Sensor *gy21Sensor;
    Matrix *matrixCurrent;
    SemaphoreHandle_t matrixMutex = nullptr;

    std::atomic<bool> enabled;
    std::atomic<bool> textEnabled;
    std::atomic<bool> backgroundEnabled;
    std::atomic<uint8_t> panelBrightness; // 0-255  

    std::atomic<int> updateIntervalMS; // polling interval in milliseconds, calculated from FPS
    std::atomic<bool> fpsChanged;

    // This TaskHandle for the update function
    TaskHandle_t updateTaskHandle = NULL;

    // draw the cells in matrix to the panel, one by one
    void drawCellsToPanel(Matrix *matrix);
    // draw text to the panel at (x,y) with given font and color
    void drawTextToPanel(char *text, int8_t x, int8_t y, const GFXfont *font, uint16_t fontColor);
    // draw all texts to the panel
    void drawAllTextToPanel();

    // the main update task function that updates the matrix display
    void updateTask();

    // a static function wrapper we can use as a task function
    static void updateTaskWrapper(void *params)
    {
        static_cast<MatrixDriver *>(params)->updateTask();
    }

    ///////////////////////
    // Text related members
    SemaphoreHandle_t temperatureTextMutex; // Protects all Temperature text-related members
    char textBufferTemperature[16];
    // for text size calculations. note we change font glyph * to °, for GFXFonts without degree symbol
    const char *maxTemperatureTextString = "99.9*";
    const GFXfont *temperatureFont;
    uint16_t temperatureFontColor;
    // position of char Temperature text on panel
    uint8_t temperatureTextX = MATRIX_WIDTH / 2;
    uint8_t temperatureTextY = MATRIX_HEIGHT / 2;
    int8_t temperatureTextXOffset = 0; // for visual centering adjustments
    int8_t temperatureTextYOffset = 0; // for visual centering adjustments

    SemaphoreHandle_t humidityTextMutex; // Protects all Humidity text-related members
    char textBufferHumidity[16];
    // for text size calculations. we changed percent symbol % to / for smaller font file
    const char *maxHumidityTextString = "55/";
    const GFXfont *humidityFont;
    uint16_t humidityFontColor;
    // position of char Humidity text on panel
    uint8_t humidityTextX = MATRIX_WIDTH / 2;
    uint8_t humidityTextY = MATRIX_HEIGHT / 2;
    int8_t humidityTextXOffset = 0; // for visual centering adjustments
    int8_t humidityTextYOffset = 0; // for visual centering adjustments
};

#endif