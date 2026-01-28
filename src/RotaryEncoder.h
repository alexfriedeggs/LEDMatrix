#ifndef ROTARYENCODER_H
#define ROTARYENCODER_H

#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include <atomic>

// default limits for encoder counts. we never get anywhere near these as we reset every read
#define DEFAULT_HIGH_LOW_LIMIT std::numeric_limits<std::int16_t>::max() / 2 
#define MAX_GLITCH_TIME 12 // maximum glitch filter time in microseconds (1023 APB clock cycles at 80MHz)

// Quadrature encoder signal waveforms:
// A      +-----+     +-----+     +-----+
//              |     |     |     |
//              |     |     |     |
//              +-----+     +-----+
// B         +-----+     +-----+     +-----+
//                 |     |     |     |
//                 |     |     |     |
//                 +-----+     +-----+

//  +--------------------------------------->
//                 CW direction

// This class uses the ESP32's Pulse Counter (PCNT) hardware to read rotary encoders.
// each full detent (click) of the encoder produces 4 counts (A and B channels both produce 2 counts per detent).
// The class configures the PCNT to count up/down based on the quadrature signals from the encoder.
// It also sets up interrupts to detect when the count reaches +4 or -4, indicating one detent has been turned.
// It latches switch presses with a GPIO interrupt and debounces them in software during polling.
// Each instance of RotaryEncoder is assigned a unique PCNT unit.
// Make sure not to exceed the number of available PCNT units on the ESP32 PCNT_UNIT_MAX (usually 8).
class RotaryEncoder
{
public:
    RotaryEncoder(int16_t glitchFilterTimeMicroS, int16_t switchDebounceTimeMS, int GPIO_A, int GPIO_B, int GPIO_SW);
    ~RotaryEncoder();

    // return current accumulated detent count since last call and reset to zero
    int getDetentCountAndReset();

    // return debounced switch state and reset to false
    bool getDebouncedSwitchStateAndReset();

    // enable or disable the encoder counting
    void enableCounter(bool enable);

    // enable or disable the switch press detection
    void enableSwitch(bool enable);

private:
    int16_t glitchFilterTimeMicroS;
    int16_t switchDebounceTimeMS;
    long lastSwitchPressTime;

    int16_t lowLimit;
    int16_t highLimit;
    pcnt_unit_t pcntUnit;

    int GPIO_A;
    int GPIO_B;
    int GPIO_SW;

    static int instanceCount;     // static count of instances to assign unique PCNT units

    std::atomic<int> detentCount; // count of detents turned since last poll
    std::atomic<bool> switchPressed; // flag to indicate if switch was pressed since last poll

    void incrementDetentCounter();
    void resetPcntCounter();

    // static ISR handler for detent counting.
    static void IRAM_ATTR isrDetentHandler(void *arg);
    // static ISR handler for switch press.
    static void IRAM_ATTR isrSwitchHandler(void *arg);
};

#endif