#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H

#pragma once

#include <Arduino.h>
#include "RotaryEncoder.h"
#include "Logger.h"
#include "MODES.h"

// MIN/MAX ADC values(0-4095) for ldr thresholds - determined experimentally
#define LDR_LOWER_ADC 600 // below this turn OFF display
#define LDR_UPPER_ADC 850 // above this turn ON display

// Handles user inputs via two rotary encoders and an LDR for ambient light sensing.
// Encoder1 sets brightness (0-255), Encoder2 sets hue (0-65535).
// An LDR input is used to enable/disable the panel based on ambient light levels.
// The class uses a polling task to read encoder movements and LDR values at a specified interval
class InputHandler
{
public:
    InputHandler(int pollingIntervalMS,
                  int ENC_BRIGHT_A, int ENC_BRIGHT_B, int ENC_BRIGHT_SW,
                  int ENC_COLOR_A, int ENC_COLOR_B, int ENC_COLOR_SW,
                  int ldrPin,
                  uint8_t minBright = 0, uint8_t maxBright = 255,
                  uint16_t minHue = 0, uint16_t maxHue = 65535,
                  int16_t glitchFilterTimeMicroS = 10, int16_t switchDebounceTimeMS = 50,
                  int startingDisplayMode = MODES::GAME_AND_TEXT,
                  int startingTextMode = MODES::TEXT_MODE_WHITE,
                  uint8_t startingBrightness = 255,
                  uint16_t startingHue = 32768);
    ~InputHandler();

    void resume();
    void pause();

    // thread-safe getter for brightness, hue, displayMode, textMode& ldrEnable
    void getState(uint8_t &brightness, uint16_t &hue, int &displayMode, int &textMode, bool &ldrEnable);

    // TESTING ONLY //////////////////////
    // : read raw LDR ADC value
    int getCurrentLDRValue() { return ldrValue; }

private:
    int ldrPin;              // LDR input
    RotaryEncoder *encoder1; // left encoder for whole numbers
    RotaryEncoder *encoder2; // right encoder for decimal numbers

    uint8_t minBright; // 0-255 (normally 0)
    uint8_t maxBright; // 0-255 (normally 255)
    uint16_t minHue;   // 0-65535
    uint16_t maxHue;   // 0-65535

    // these variables can be accessed inside and outside the task, so are atomic
    std::atomic<uint8_t> brightness; // brightness level (0-255)
    std::atomic<uint16_t> hue;       // hue value (0-65535)
    std::atomic<int> displayMode;    // current mode set by encoder
    std::atomic<int> textMode;       // secondary mode if needed
    std::atomic<bool> ldrEnabled;    // current panel enabled state based on LDR
    SemaphoreHandle_t inputMutex;    // used when we want to read/update multiple atomics at once

    // unrealistic initial value to force first read updates
    int ldrValue = -10000; // current LDR ADC value (0-4095)
    bool calcLDREnable();  // calculate LDR enable state

    std::atomic<bool> pollingEnabled; // main polling enable flag
    int pollingIntervalMS;            // polling interval in milliseconds

    // This TaskHandle for the polling function
    TaskHandle_t pollingTaskHandle = NULL;

    // the main polling task function that polls encoders rotation and switches,
    // resets counters, boundchecks, acceleration then updates values accordingly
    void pollingTask();

    // a static function wrapper we can use as a task function
    static void pollingTaskWrapper(void *params)
    {
        static_cast<InputHandler *>(params)->pollingTask();
    }
};

#endif