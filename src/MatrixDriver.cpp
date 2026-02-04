#include "MatrixDriver.h"

MatrixDriver::MatrixDriver(int fps, Panel *panel, Matrix *matrix, GY21Sensor *gy21Sensor,
                           const GFXfont *temperatureFont, const GFXfont *humidityFont,
                           uint16_t temperatureFontColor, uint16_t humidityFontColor)
{
    this->panel = panel;
    this->matrixCurrent = matrix;
    this->gy21Sensor = gy21Sensor;
    this->setPanelBrightness(255); // default brightness
    this->enabled.store(false);
    this->textEnabled.store(true);
    this->backgroundEnabled.store(true);

    // initialise brightness to sync values. brightness normally set by potentiometer in runtime
    if (panel != nullptr)
    {
        panel->setBrightness(panelBrightness);
    }

    fps = constrain(fps, 1, MAX_FPS);
    this->updateIntervalMS.store(1000 / fps);
    this->fpsChanged.store(true); // so we calc timing on first frame

    // Create mutexes for text properties
    temperatureTextMutex = xSemaphoreCreateMutex();
    if (temperatureTextMutex == NULL)
    {
        Logger::println("ERROR: Failed to create temperatureTextMutex");
    }
    humidityTextMutex = xSemaphoreCreateMutex();
    if (humidityTextMutex == NULL)
    {
        Logger::println("ERROR: Failed to create humidityTextMutex");
    }

    // Create mutex for matrix pointer access
    matrixMutex = xSemaphoreCreateMutex();
    if (matrixMutex == NULL)
    {
        Logger::println("ERROR: Failed to create matrixMutex");
    }

    if (temperatureFont == nullptr) // default font
        this->temperatureFont = &DEFAULT_FONT;
    else
        this->temperatureFont = temperatureFont;
    this->temperatureFontColor = temperatureFontColor;
    this->setTemperatureFont(this->temperatureFont);
    this->setTemperatureFontColor(this->temperatureFontColor);
    this->setTemperatureText("");

    if (humidityFont == nullptr) // default font
        this->humidityFont = &DEFAULT_FONT;
    else
        this->humidityFont = humidityFont;
    this->humidityFontColor = humidityFontColor;
    this->setHumidityFont(this->humidityFont);
    this->setHumidityFontColor(this->humidityFontColor);
    this->setHumidityText("");

    // create a task to handle the update in the background
    xTaskCreatePinnedToCore(
        MatrixDriver::updateTaskWrapper, // Function that should be called
        "Matrix Update Task",            // Name of the task (for debugging)
        10000,                           // Stack size (bytes)
        this,                            // Parameter to pass
        3,                               // Task priority - low priority for sensor updates
        &updateTaskHandle,               // Task handle
        1                                // Core to run the task on (0 or 1)
    );
    delay(100);
    if (updateTaskHandle == NULL)
    {
        Logger::println("Failed to create MatrixDriver updateTask");
    }
    else
    {
        // pause the task till it's needed
        Logger::println("MatrixDriver update task created, suspending it");
        pause();
    }
}

// manually update the desired FPS
void MatrixDriver::setFPS(int fps)
{
    fps = constrain(fps, 1, MAX_FPS);
    this->updateIntervalMS.store(1000 / fps);
    this->fpsChanged.store(true); // flag for task to recalc timing
    Logger::printf("MatrixDriver:updateIntervalMS set to %d ms", this->updateIntervalMS.load());
}

// set panel brightness 0-255..note this is only applied to panel in the update task
void MatrixDriver::setPanelBrightness(uint8_t brightness)
{
    this->panelBrightness.store(brightness);
}

// enable/disable text drawing
void MatrixDriver::enableTextDrawing(bool enable)
{
    this->textEnabled.store(enable);
}

// enable/disable background drawing
void MatrixDriver::enableBackgroundDrawing(bool enable)
{
    this->backgroundEnabled.store(enable);
    // if turning off background drawing, ensure we clear screen first
    if (xSemaphoreTake(matrixMutex, portMAX_DELAY) == pdTRUE)
    {
        panel->clearScreen();
        xSemaphoreGive(matrixMutex);
    }
}

// safely set a new matrix to use
void MatrixDriver::setMatrix(Matrix *newMatrix)
{
    if (!newMatrix)
        return;
    if (matrixMutex == NULL)
    {
        Logger::println("WARNING: matrixMutex not created; setting matrix without lock");
        matrixCurrent = newMatrix;
        return;
    }
    if (xSemaphoreTake(matrixMutex, portMAX_DELAY) == pdTRUE)
    {
        matrixCurrent = newMatrix;
        xSemaphoreGive(matrixMutex);
    }
}

// the main update task function that updates the matrix display
// order of operations:
// 1. SYNC TO RTOS TIMING
// 2. HANDLE PAUSE/RESUME AND SCREEN CLEARING
// 3. SET MATRIX POINTER SAFELY
// 4. FLIP BUFFERS SO BACK BUFFER PUSHED TO DISPLAY AND WE DRAW TO THE BACK BUFFER
// 5. WAIT FOR FPS DELAY HERE - AT LEAST ONE FULL REFRESH
// 6. CLEAR BACK BUFFER
// 7. UPDATE PANEL BRIGHTNESS IF NEEDED
// 8. READ INPUTS
// 9. CALC NEW MATRIX STATES
// 10. DRAW CELLS TO BACK BUFFER
// 11. DRAW TEXT TO BACK BUFFER
// 12. TIMING LOGGING
void MatrixDriver::updateTask()
{
    bool wasEnabled = false;

    // Timing variables
    TickType_t lastWake = xTaskGetTickCount();
    // Desired animation frame period
    TickType_t framePeriod;
    // Physical panel limitation
    TickType_t minSwapPeriod;
    // Never flip buffers faster than the panel can display them,& never update slower than the requested FPS.
    TickType_t effectivePeriod;
    unsigned long tStart, tBuffering, tRead, tCalc, tDraw, tText;

    while (true)
    {
        // 1. SYNC TO RTOS TIMING
        // check if fpsChanged has changed and set fpsChanged to false
        if (fpsChanged.exchange(false))
        {
            // recalc effective period
            framePeriod = pdMS_TO_TICKS(updateIntervalMS.load());
            // Physical panel limitation
            minSwapPeriod = pdMS_TO_TICKS(1000 / panel->getCalculatedRefreshRate());
            // Never flip buffers faster than the panel can display them,& never update slower than the requested FPS.
            effectivePeriod = (minSwapPeriod > framePeriod) ? minSwapPeriod : framePeriod;
        }

        // waiting for enable signal
        if (!enabled.load())
        {
            // 2. HANDLE PAUSE/RESUME AND SCREEN CLEARING
            // if just disabled, then set brightness to 0 and clear both buffers.
            if (wasEnabled)
            {
                Logger::println("MatrixDriver paused, clearing panel");
                // disable panel output before clearing
                panel->setBrightness(0);

                // clear both buffers for clean restart
                if (panel->isDoubleBuffered())
                {
                    // clear both buffers for clean restart
                    panel->clearScreen();
                    panel->swapDMABuffers();
                    // 3. WAIT FOR AT LEAST ONE FULL REFRESH AFTER BUFFER SWAP
                    vTaskDelayUntil(&lastWake, effectivePeriod);
                    panel->clearScreen();
                }
                else
                {
                    panel->clearScreen();
                }
            }

            wasEnabled = false;
            vTaskDelay(pdMS_TO_TICKS(150)); // coarse sleep while paused
            lastWake = xTaskGetTickCount(); // reset schedule
            continue;                       // skip to next iteration of while loop
        }

        // waking up from disabled...
        if (!wasEnabled)
        {
            Logger::println("MatrixDriver resumed from paused");
            // on reenable, restore brightness. we start with both buffers blank
            panel->setBrightness(panelBrightness.load());
            lastWake = xTaskGetTickCount(); // prevent “catch up”
            wasEnabled = true;
        }

        /////////////////////////////////
        // Normal update operations below
        // 3. SET MATRIX POINTER SAFELY
        Matrix *matrix = nullptr;
        if (matrixMutex == NULL)
        {
            Logger::println("MatrixDriver: WARNING: matrixMutex not created; using matrix without lock");
            matrix = this->matrixCurrent;
        }
        else if (xSemaphoreTake(matrixMutex, portMAX_DELAY) == pdTRUE)
        {
            matrix = this->matrixCurrent;
            xSemaphoreGive(matrixMutex);
        }

        if (!matrix)
        {
            vTaskDelay(1);
            continue;
        }

        // 4. FLIP BUFFERS SO BACK BUFFER PUSHED TO DISPLAY AND WE DRAW TO THE BACK BUFFER
        if (panel->isDoubleBuffered())
        {
            panel->swapDMABuffers();
        }

        // 5. WAIT FOR FPS DELAY HERE - AT LEAST ONE FULL REFRESH
        // Wait until the next frame boundary. This enforces the effective FPS *and* ensures the DMA engine
        // has completed at least one full panel refresh before we write to the back buffer.
        // This is important to avoid visual tearing due to DMA reading from buffer while we write to it
        vTaskDelayUntil(&lastWake, effectivePeriod);

        tStart = micros(); // start timing after delay
        // 6. CLEAR BACK BUFFER
        panel->clearScreen();
        tBuffering = micros();

        // 7. UPDATE PANEL BRIGHTNESS IF NEEDED
        if (panel->getBrightness() != panelBrightness.load())
        {
            panel->setBrightness(panelBrightness.load());
        }

        // 8. READ INPUTS
        // if temp or humidity has changed since last read, then update text
        if (gy21Sensor->hasValueChanged())
        {
            char temporaryTextBuffer[16];
            char temporaryHumidityBuffer[16];

            gy21Sensor->getTemperatureString(temporaryTextBuffer, sizeof(temporaryTextBuffer));
            gy21Sensor->getHumidityString(temporaryHumidityBuffer, sizeof(temporaryHumidityBuffer));
            setTemperatureText(temporaryTextBuffer);
            setHumidityText(temporaryHumidityBuffer);
        }
        tRead = micros();

        // if background drawing is enabled, then update matrix states & draw cells
        if (backgroundEnabled.load())
        {
            // 9. CALC NEW MATRIX STATES
            matrix->calcNewStates();
            tCalc = micros();

            // 10. DRAW CELLS TO BACK BUFFER
            drawCellsToPanel(matrix);
            tDraw = micros();
        }
        else // background remains cleared (black)
        {
            tCalc = micros();
            tDraw = tCalc;
        }

        // 11. DRAW TEXT TO BACK BUFFER IF ENABLED
        if (textEnabled.load())
        {
            drawAllTextToPanel();
        }
        tText = micros();

        // 12. TIMING LOGGING
        static uint updateEveryNFrames = 60;
        static uint16_t frameCount = 0;
        static unsigned long lastFrameTime = 0;

        if (++frameCount >= updateEveryNFrames)
        {
            frameCount = 0;
            unsigned long workTime = tText - tStart;
            unsigned long totalFrameTime = tStart - lastFrameTime;
            unsigned long idleTime = (totalFrameTime > workTime) ? (totalFrameTime - workTime) : 0;
            float actualFPS = (lastFrameTime > 0) ? (1000000.0f / totalFrameTime) : 0.0f;

            Logger::printf("Timing (µs) - Calc: %lu, Draw: %lu, Text: %lu, Total Work: %lu\n, Idle: %lu, Frame Total: %lu, Actual FPS: %.1f, Idle%%: %.1f%%\n",
                           tCalc - tRead,
                           tDraw - tCalc,
                           tText - tDraw,
                           workTime,
                           idleTime,
                           totalFrameTime,
                           actualFPS,
                           (totalFrameTime > 0) ? (100.0f * idleTime / totalFrameTime) : 0.0f);
        }
        lastFrameTime = tStart;
    }
}

// pause update task safely.
void MatrixDriver::pause()
{
    enabled.store(false);
}

// resume update task safely,
void MatrixDriver::resume()
{
    enabled.store(true);
}

// draw all the cells from the matrix to the panel
void MatrixDriver::drawCellsToPanel(Matrix *matrix)
{
    // draw to panel
    for (int x = 0; x < MAT_WIDTH; x++)
    {
        for (int y = 0; y < MAT_HEIGHT; y++)
        {
            panel->drawPixel(x, y, matrix->getCellColor(x, y));
        } // end for y
    } // end for x
}

// draw all the temperature and humidity text to the panel
void MatrixDriver::drawAllTextToPanel()
{
    // draw temperature text from GY21Sensor
    if (xSemaphoreTake(temperatureTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        // draw temperature text from GY21Sensor
        drawTextToPanel(textBufferTemperature, temperatureTextX + temperatureTextXOffset,
                        temperatureTextY + temperatureTextYOffset, temperatureFont, temperatureFontColor);
        xSemaphoreGive(temperatureTextMutex);
    }
    else
    {
        Logger::println("WARNING: drawTemperatureText timeout");
    }
    // draw humidity text from GY21Sensor
    if (xSemaphoreTake(humidityTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {

        drawTextToPanel(textBufferHumidity, humidityTextX + humidityTextXOffset,
                        humidityTextY + humidityTextYOffset, humidityFont, humidityFontColor);
        xSemaphoreGive(humidityTextMutex);
    }
    else
    {
        Logger::println("WARNING: drawHumidityText timeout");
    }
}

// draw text to panel at (x,y) with given font and color
void MatrixDriver::drawTextToPanel(char *text, int8_t x, int8_t y, const GFXfont *font, uint16_t fontColor)
{
    panel->setFont(font);
    panel->printText(text, x, y, fontColor);
}

void MatrixDriver::setTemperatureText(const char *text)
{
    if (xSemaphoreTake(temperatureTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        strncpy(textBufferTemperature, text, sizeof(textBufferTemperature) - 1);
        textBufferTemperature[sizeof(textBufferTemperature) - 1] = '\0';
        xSemaphoreGive(temperatureTextMutex);
    }
    else
    {
        Logger::println("WARNING: setText timeout");
    }
}

void MatrixDriver::setTemperatureTextPosition(uint8_t x, uint8_t y)
{
    if (xSemaphoreTake(temperatureTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        temperatureTextX = x;
        temperatureTextY = y;
        xSemaphoreGive(temperatureTextMutex);
    }
}

void MatrixDriver::setTemperatureTextXOffset(int8_t xOffset)
{
    if (xSemaphoreTake(temperatureTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        temperatureTextXOffset = xOffset;
        xSemaphoreGive(temperatureTextMutex);
    }
}

void MatrixDriver::setTemperatureTextYOffset(int8_t yOffset)
{
    if (xSemaphoreTake(temperatureTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        temperatureTextYOffset = yOffset;
        xSemaphoreGive(temperatureTextMutex);
    }
}
// set font for text drawing
void MatrixDriver::setTemperatureFont(const GFXfont *font)
{
    if (xSemaphoreTake(temperatureTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        this->temperatureFont = font;
        // also update text size calculations
        panel->setFont(this->temperatureFont);
        // centre middle
        temperatureTextX = (MATRIX_WIDTH - panel->getTextWidth(String(maxTemperatureTextString))) / 2;
        temperatureTextY = (MATRIX_HEIGHT + panel->getTextHeight(String(maxTemperatureTextString))) / 2;
        xSemaphoreGive(temperatureTextMutex);
    }
}

void MatrixDriver::setTemperatureFontColor(uint16_t color)
{
    if (xSemaphoreTake(temperatureTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        this->temperatureFontColor = color;
        xSemaphoreGive(temperatureTextMutex);
    }
}

void MatrixDriver::setHumidityText(const char *text)
{
    if (xSemaphoreTake(humidityTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        strncpy(textBufferHumidity, text, sizeof(textBufferHumidity) - 1);
        textBufferHumidity[sizeof(textBufferHumidity) - 1] = '\0';
        xSemaphoreGive(humidityTextMutex);
    }
    else
    {
        Logger::println("WARNING: setHumidityText timeout");
    }
}

void MatrixDriver::setHumidityTextPosition(uint8_t x, uint8_t y)
{
    if (xSemaphoreTake(humidityTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        humidityTextX = x;
        humidityTextY = y;
        xSemaphoreGive(humidityTextMutex);
    }
}

void MatrixDriver::setHumidityTextXOffset(int8_t xOffset)
{
    if (xSemaphoreTake(humidityTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        humidityTextXOffset = xOffset;
        xSemaphoreGive(humidityTextMutex);
    }
}

void MatrixDriver::setHumidityTextYOffset(int8_t yOffset)
{
    if (xSemaphoreTake(humidityTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        humidityTextYOffset = yOffset;
        xSemaphoreGive(humidityTextMutex);
    }
}
// set font for text drawing
void MatrixDriver::setHumidityFont(const GFXfont *font)
{
    if (xSemaphoreTake(humidityTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        this->humidityFont = font;
        // also update text size calculations
        panel->setFont(this->humidityFont);
        // centre bottom
        humidityTextX = (MATRIX_WIDTH - panel->getTextWidth(String(maxHumidityTextString))) / 2;
        humidityTextY = MATRIX_HEIGHT - panel->getTextHeight(String(maxHumidityTextString));
        Logger::printf("Humidity font set. Text width: %d, height: %d\n",
                       panel->getTextWidth(String(maxHumidityTextString)),
                       panel->getTextHeight(String(maxHumidityTextString)));
        xSemaphoreGive(humidityTextMutex);
    }
}

void MatrixDriver::setHumidityFontColor(uint16_t color)
{
    if (xSemaphoreTake(humidityTextMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        this->humidityFontColor = color;
        xSemaphoreGive(humidityTextMutex);
    }
}

MatrixDriver::~MatrixDriver()
{
    if (matrixMutex != NULL)
    {
        vSemaphoreDelete(matrixMutex);
    }
    if (temperatureTextMutex != NULL)
    {
        vSemaphoreDelete(temperatureTextMutex);
    }
    if (humidityTextMutex != NULL)
    {
        vSemaphoreDelete(humidityTextMutex);
    }
}