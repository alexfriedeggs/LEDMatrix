#include "InputHandler.h"

InputHandler::InputHandler(int pollingIntervalMS,
                             int ENC_BRIGHT_A, int ENC_BRIGHT_B, int ENC_BRIGHT_SW,
                             int ENC_COLOR_A, int ENC_COLOR_B, int ENC_COLOR_SW,
                             int ldrPin,
                             uint8_t minBright, uint8_t maxBright,
                             uint16_t minHue, uint16_t maxHue,
                             int16_t glitchFilterTimeMicroS, int16_t switchDebounceTimeMS,
                             int startingDisplayMode, int startingTextMode,
                             uint8_t startingBrightness, uint16_t startingHue)
{
    this->pollingEnabled = false;
    this->ldrPin = ldrPin;
    this->minBright = minBright;
    this->maxBright = maxBright;
    this->minHue = minHue;
    this->maxHue = maxHue;

    this->pollingIntervalMS = pollingIntervalMS;

    this->brightness.store(startingBrightness);
    this->hue.store(startingHue);
    this->displayMode.store(startingDisplayMode);
    this->textMode.store(startingTextMode);
    this->ldrEnabled.store(true);

    // Create mutex for inputs
    inputMutex = xSemaphoreCreateMutex();
    if (inputMutex == NULL)
    {
        Logger::println("ERROR: Failed to create inputMutex");
    }

    // create encoders
    // restrict glitch filter time to 0 to 12us (1023 APB clock cycles at 80MHz)
    glitchFilterTimeMicroS = constrain(glitchFilterTimeMicroS, 0, 12);
    // encoder1 for brightness and mode setting
    encoder1 = new RotaryEncoder(glitchFilterTimeMicroS, switchDebounceTimeMS,
                                 ENC_BRIGHT_A, ENC_BRIGHT_B, ENC_BRIGHT_SW);
    // encoder2 for color hue setting and secondary mode if needed
    encoder2 = new RotaryEncoder(glitchFilterTimeMicroS, switchDebounceTimeMS,
                                 ENC_COLOR_A, ENC_COLOR_B, ENC_COLOR_SW);

    // create a low-priority task to handle the encoder polling in the background
    xTaskCreatePinnedToCore(
        InputHandler::pollingTaskWrapper, // Function that should be called
        "InputHandler Polling Task",       // Name of the task (for debugging)
        10000,                             // Stack size (bytes)
        this,                              // Parameter to pass
        3,                                 // Task priority
        &pollingTaskHandle,                // Task handle
        1                                  // Core to run the task on (0 or 1)
    );

    if (pollingTaskHandle == NULL)
    {
        Logger::println("Failed to create InputHandler pollingTaskHandle");
    }
    else
    {
        // make sure everything reset before we start
        encoder1->getDetentCountAndReset();
        encoder2->getDetentCountAndReset();
        encoder1->getDebouncedSwitchStateAndReset();
        encoder2->getDebouncedSwitchStateAndReset();
        // pause the task till it's needed
        Logger::println("InputHandler polling task created, suspending it");
        pause();
    }
}

void InputHandler::pause()
{
    pollingEnabled.store(false);
}
void InputHandler::resume()
{
    pollingEnabled.store(true);
}

// the main polling task function that polls encoders rotation and switches,
// resets counters, boundchecks, acceleration
// then updates values accordingly
void InputHandler::pollingTask()
{
    while (true)
    {
        if (pollingEnabled.load())
        {
            // we use temporary variables to hold current frequency components
            uint8_t tempBright;
            uint16_t tempHue;
            int tempDisplayMode;
            int tempTextMode;
            bool tempLdrEnable;
            getState(tempBright, tempHue, tempDisplayMode, tempTextMode, tempLdrEnable);

            // first read ldr pin & update LDR enable state
            tempLdrEnable = calcLDREnable();

            // handle switch presses for encoder1 - Mode select
            // cycle through ranges on switch press
            if (encoder1->getDebouncedSwitchStateAndReset())
            {
                // increment mode and wrap around
                tempDisplayMode = (tempDisplayMode + 1) % MODES::TOTAL_MODES;
                Logger::printf("Mode Select button pressed. New mode: %d\n", tempDisplayMode);
            }
            // now check encoder2 switch press for secondary mode select
            if (encoder2->getDebouncedSwitchStateAndReset())
            {
                tempTextMode = ( ((tempTextMode + 1) % 2)+ 10);
                Logger::printf("Mode2 Select button pressed. New mode2: %d\n", tempTextMode);
            }

            //  first calc new values based on encoder detent counts since last poll
            int dCount1 = encoder1->getDetentCountAndReset();
            int dCount2 = encoder2->getDetentCountAndReset();

            // acceleration steps for encoders
            int accel1 = dCount1 * dCount1 * (dCount1 < 0 ? -1 : 1); // brightness squared for acceleration (0-255 range)
            int accel2 = 8 * dCount2 * dCount2 * dCount2;            // hue cubed for acceleration (0-65535 range)

            // cast to unsigned int to avoid overflows when adding acceleration e.g. 255 + 16
            int b = (int)tempBright;
            // here we use the overflow to our advantage for hue wrapping
            uint16_t h = tempHue;
            b += accel1;
            h += accel2;
            // apply bounds-checking
            b = constrain(b, (int)minBright, (int)maxBright);
            // cast back to correct type
            tempBright = (uint8_t)b;
            tempHue = h;

            // THIS MAY BE OVERKILL IN THIS INSTANCE....
            // finally update the atomic components all at once with mutex protection
            xSemaphoreTake(inputMutex, portMAX_DELAY);
            brightness.store(tempBright);
            hue.store(tempHue);
            displayMode.store(tempDisplayMode);
            textMode.store(tempTextMode);
            ldrEnabled.store(tempLdrEnable);
            xSemaphoreGive(inputMutex);
        }
        // delay for polling interval //this doesn't need to be very precise for input handling
        vTaskDelay(pdMS_TO_TICKS(pollingIntervalMS));
    }
}

// thread-safe getter for current values
void InputHandler::getState(uint8_t &brightness, uint16_t &hue, int &displayMode, int &textMode, bool &ldrEnable)
{
    // acquire mutex to ensure consistent read across all variables
    xSemaphoreTake(inputMutex, portMAX_DELAY);

    brightness = this->brightness.load();
    hue = this->hue.load();
    displayMode = this->displayMode.load();
    textMode = this->textMode.load();
    ldrEnable = this->ldrEnabled.load();

    xSemaphoreGive(inputMutex);
}

// read LDR input. determine if panel should be latched enabled or disabled
// true = panel enabled, false = panel disabled
bool InputHandler::calcLDREnable()
{
    // read and store actual LDR value for threshold checking
    ldrValue = analogRead(ldrPin); // (0-4095 ADC)

    // determine if panel should be enabled or disabled based on thresholds
    if (ldrValue < LDR_LOWER_ADC)
    {
        ldrEnabled.store(false);
    }
    else if (ldrValue > LDR_UPPER_ADC)
    {
        ldrEnabled.store(true);
    }
    // return current latched state
    return ldrEnabled.load();
}

InputHandler::~InputHandler()
{
}