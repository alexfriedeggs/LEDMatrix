#include <RotaryEncoder.h>
#include "Logger.h"

int RotaryEncoder::instanceCount = 0;

RotaryEncoder::RotaryEncoder(int16_t glitchFilterTimeMicroS, int16_t switchDebounceTimeMS,
                             int GPIO_A, int GPIO_B, int GPIO_SW)
{
    // Assign a PCNT unit based on the instance count
    // e.g. first instance gets PCNT_UNIT_0, second gets PCNT_UNIT_1, etc.
    // Make sure not to exceed the number of available PCNT units on the ESP32 PCNT_UNIT_MAX (usually 8).
    if (instanceCount >= PCNT_UNIT_MAX)
    {
        // Handle error: too many encoders
        this->pcntUnit = PCNT_UNIT_MAX; // invalid unit
        return;
    }
    this->pcntUnit = static_cast<pcnt_unit_t>(instanceCount);
    instanceCount++;

    // restrict glitch filter time to 0 to 12us (1023 APB clock cycles at 80MHz)
    this->glitchFilterTimeMicroS = constrain(glitchFilterTimeMicroS, 0, MAX_GLITCH_TIME);

    this->switchDebounceTimeMS = switchDebounceTimeMS;
    this->lastSwitchPressTime = 0;
    this->lowLimit = -DEFAULT_HIGH_LOW_LIMIT;
    this->highLimit = DEFAULT_HIGH_LOW_LIMIT;
    this->GPIO_A = GPIO_A;
    this->GPIO_B = GPIO_B;
    this->GPIO_SW = GPIO_SW;
    
    // pull-ups for encoder pins
    gpio_set_direction((gpio_num_t)GPIO_A, GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)GPIO_B, GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)GPIO_SW, GPIO_MODE_INPUT);
    gpio_pullup_en((gpio_num_t)GPIO_A);
    gpio_pullup_en((gpio_num_t)GPIO_B);
    gpio_pullup_en((gpio_num_t)GPIO_SW);
    gpio_pulldown_dis((gpio_num_t)GPIO_SW);

    // Prepare configuration for the PCNT unit channel 0
    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = GPIO_A,
        .ctrl_gpio_num = GPIO_B,
        // What to do when control input is low or high?
        .lctrl_mode = PCNT_MODE_REVERSE, // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_DEC, // Count down on the positive edge
        .neg_mode = PCNT_COUNT_INC, // Count up on the negative edge
        // Set the maximum and minimum limit values to watch
        .counter_h_lim = highLimit,
        .counter_l_lim = lowLimit,
        .unit = pcntUnit,
        .channel = PCNT_CHANNEL_0,
    };
    // Initialize PCNT unit channel 0
    pcnt_unit_config(&pcnt_config);

    // Prepare configuration for the PCNT unit channel 1
    pcnt_config.pulse_gpio_num = GPIO_B;
    pcnt_config.ctrl_gpio_num = GPIO_A;
    pcnt_config.channel = PCNT_CHANNEL_1;
    pcnt_config.pos_mode = PCNT_COUNT_INC;
    pcnt_config.neg_mode = PCNT_COUNT_DEC;
    // Initialize PCNT unit channel 1
    pcnt_unit_config(&pcnt_config);

    // Configure and enable the input filter
    uint16_t filterValue = 80 * glitchFilterTimeMicroS; // Filter value in APB (80 MHz) clock cycles
    if (filterValue > 1023)
        filterValue = 1023; // Max filter value
    pcnt_set_filter_value(pcntUnit, filterValue);
    pcnt_filter_enable(pcntUnit);

    // Pause and clear PCNT's counter so we have a known zero value
    pcnt_counter_pause(pcntUnit);
    pcnt_counter_clear(pcntUnit);

    // enable events on reaching +-4 counts (one detent in either direction)
    pcnt_set_event_value(pcntUnit, PCNT_EVT_THRES_1, 4);
    pcnt_set_event_value(pcntUnit, PCNT_EVT_THRES_0, -4);
    pcnt_event_enable(pcntUnit, PCNT_EVT_THRES_1); // +4 threshold
    pcnt_event_enable(pcntUnit, PCNT_EVT_THRES_0); // -4 threshold

    // Install pcnt interrupt service once for all instances
    static bool pcntISRServiceInstalled = false;
    if (!pcntISRServiceInstalled)
    {
        pcnt_isr_service_install(0);
        pcntISRServiceInstalled = true;
    }

    // Enable interrupts and add the isr handler for this instance of RotaryEncoder
    pcnt_intr_enable(pcntUnit);
    pcnt_isr_handler_add(pcntUnit, isrDetentHandler, (void *)this);

    // Restart the counter to start counting from 0
    pcnt_counter_resume(pcntUnit);
    pcnt_counter_clear(pcntUnit);

    // Install gpio interrupt service once for all instances
    static bool gpioISRServiceInstalled = false;
    if (!gpioISRServiceInstalled)
    {
        gpio_install_isr_service(0);
        gpioISRServiceInstalled = true;
    }

    // add interrupt to the switch pin, detecting falling edge from pullup
    // the isr takes as argument a pointer to this instance
    gpio_set_intr_type(static_cast<gpio_num_t>(GPIO_SW), GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(static_cast<gpio_num_t>(GPIO_SW), isrSwitchHandler, (void *)this);
}

// ISR handler for detent counting. called every 4 counts in either direction,
// this is common to all instances, it takes as argument
// a pointer to the instance of the RotaryEncoder that triggered the interrupt
void IRAM_ATTR RotaryEncoder::isrDetentHandler(void *arg)
{
    RotaryEncoder *encoder = static_cast<RotaryEncoder *>(arg);
    // Increment detent count up or down based on encoder movement and reset the pcnt counter before returning
    encoder->incrementDetentCounter();
    encoder->resetPcntCounter();
}

// ISR handler for switch press. called on falling edge of switch pin.
// this simply latches switchPressed to true. the state can only be reset with an external read
void IRAM_ATTR RotaryEncoder::isrSwitchHandler(void *arg)
{
    RotaryEncoder *encoder = static_cast<RotaryEncoder *>(arg);
    int level = gpio_get_level(static_cast<gpio_num_t>(encoder->GPIO_SW));
    if (level != 0)
        return; // ignore if not actually low (debounce in hardware)
    encoder->switchPressed.store(true);
}

// Increment the detent counter based on encoder rotation
void RotaryEncoder::incrementDetentCounter()
{
    // determine direction based on PCNT count and increment/decrement detentCount by 1
    int16_t count = 0;
    pcnt_get_counter_value(pcntUnit, &count);
    if (count >= 4)
        detentCount++;
    else if (count <= -4)
        detentCount--;
}

// Reset the PCNT counter to zero
void RotaryEncoder::resetPcntCounter()
{
    pcnt_counter_clear(pcntUnit);
}

// return current accumulated detent count since last call and reset to zero
int RotaryEncoder::getDetentCountAndReset()
{
    return detentCount.exchange(0); // return current count and reset to zero atomically
}

// return debounced switch state and reset to false
bool RotaryEncoder::getDebouncedSwitchStateAndReset()
{
    // atomically get current state and reset to false
    bool currentSwitchState = switchPressed.exchange(false);
    if (!currentSwitchState)
        return false; // no press detected. no debounce needed


    if (gpio_get_level(static_cast<gpio_num_t>(GPIO_SW)) != 0)
        return false; // ignore if not actually low

    // debounce the switch press based on time since last press
    long currentTime = millis();
    if (currentSwitchState)
    {
        if (currentTime - lastSwitchPressTime >= switchDebounceTimeMS)
        // debounce time exceeded, accept this press as valid user input
        {
            lastSwitchPressTime = currentTime;
        }
        else
        {
            // too soon since last press, ignore this press
            currentSwitchState = false;
        }
    }

    return currentSwitchState;
}

// enable or disable the encoder counting. Reset counts to zero when changing state.
void RotaryEncoder::enableCounter(bool enable)
{
    if (enable)
    {
        pcnt_counter_resume(pcntUnit);
        detentCount.store(0);
        pcnt_counter_clear(pcntUnit);
    }
    else
    {
        pcnt_counter_pause(pcntUnit);
        detentCount.store(0);
        pcnt_counter_clear(pcntUnit);
    }
}

// enable or disable the switch press detection, interrupts and reset states
void RotaryEncoder::enableSwitch(bool enable)
{
    if (enable)
    {
        // resume interrupt on switch pin
        gpio_set_intr_type(static_cast<gpio_num_t>(GPIO_SW), GPIO_INTR_NEGEDGE);
        gpio_isr_handler_add(static_cast<gpio_num_t>(GPIO_SW), isrSwitchHandler, (void *)this);
        gpio_intr_enable(static_cast<gpio_num_t>(GPIO_SW));
        switchPressed.store(false);
        lastSwitchPressTime = 0;
    }
    else
    {
        // pause interrupt on switch pin
        gpio_isr_handler_remove(static_cast<gpio_num_t>(GPIO_SW));
        gpio_intr_disable(static_cast<gpio_num_t>(GPIO_SW));
        switchPressed.store(false);
        lastSwitchPressTime = 0;
    }
}

RotaryEncoder::~RotaryEncoder()
{
    // detach switch interrupt
    gpio_isr_handler_remove(static_cast<gpio_num_t>(GPIO_SW));
    // disable and remove PCNT interrupt and handler
    pcnt_intr_disable(pcntUnit);                    // Stop further interrupts from this unit.
    pcnt_isr_handler_remove(pcntUnit);              // Detach the ISR handler.
    pcnt_event_disable(pcntUnit, PCNT_EVT_THRES_0); // Disable the threshold events.
    pcnt_event_disable(pcntUnit, PCNT_EVT_THRES_1);
    instanceCount--;
    if (instanceCount <= 0)
        pcnt_isr_service_uninstall(); // Uninstall the ISR service if no more instances exist.
}