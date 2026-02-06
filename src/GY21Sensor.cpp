#include "GY21Sensor.h"

GY21Sensor::GY21Sensor(int sda, int scl, int updateIntervalMS)
{
    this->temperature.store(0.0f);
    this->humidity.store(0.0f);
    this->prevTemp.store(-1000.0f);     // unrealistic initial value
    this->prevHumidity.store(-1000.0f); // unrealistic initial value
    this->valueChanged.store(true);
    snprintf(temperatureString, sizeof(temperatureString), "00.0*");
    snprintf(humidityString, sizeof(humidityString), "00%%");

    // Create mutex for strings
    sensorStringMutex = xSemaphoreCreateMutex();
    if (sensorStringMutex == NULL)
    {
        Logger::println("ERROR: Failed to create GY21Sensor stringMutex");
    }

    this->enabled.store(false);
    this->updateIntervalMS = updateIntervalMS;

    Wire.begin(sda, scl);
    gy21.begin();

    uint8_t stat = gy21.getStatus();
    Logger::printf("GY21 Sensor initialized. Status register: 0x%02X\n", stat);

    // create a task to handle the update in the background
    xTaskCreatePinnedToCore(
        GY21Sensor::updateTaskWrapper, // Function that should be called
        "GY21Sensor Update Task",      // Name of the task (for debugging)
        10000,                         // Stack size (bytes)
        this,                          // Parameter to pass
        1,                             // Task priority // lowest
        &updateTaskHandle,             // Task handle
        1                              // Core to run the task on (0 or 1)
    );

    delay(100);

    if (updateTaskHandle == NULL)
    {
        Logger::println("Failed to create GY21SensorupdateTask");
    }
    else
    {
        // pause the task till it's needed
        Logger::println("GY21Sensor updateTask created, suspending it");
        pause();
    }
}

void GY21Sensor::updateTask()
{
    const TickType_t period = pdMS_TO_TICKS(updateIntervalMS);
    TickType_t lastWake = xTaskGetTickCount();
    bool wasEnabled = false;

    while (true)
    {
        if (!enabled.load())
        {
            wasEnabled = false;
            vTaskDelay(pdMS_TO_TICKS(150)); // coarse sleep while paused
            lastWake = xTaskGetTickCount(); // reset schedule
            continue;
        }
        if (!wasEnabled)
        {
            lastWake = xTaskGetTickCount(); // prevent “catch up”
            wasEnabled = true;
        }

        // if enabled, read sensor, update values, create strings
        readSensor();

        // stable frame pacing
        vTaskDelayUntil(&lastWake, period);
    }
}

// read sensor, update values if changed and create strings
void GY21Sensor::readSensor()
{
    if (gy21.read())
    {
        float newTemp = gy21.getTemperature() + CALIBRATION_OFFSET_TEMP; // apply calibration offset
        float newHumidity = gy21.getHumidity();

        // only update if values have changed significantly
        // we also create the strings here, if needed
        if (fabs(newTemp - temperature.load()) >= MIN_TEMP_CHANGE)
        {
            prevTemp.store(temperature.load());
            temperature.store(newTemp);
            if (xSemaphoreTake(sensorStringMutex, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                // we've remapped the degree symbol in the font to '*' for easier display on LED matrix
                snprintf(temperatureString, sizeof(temperatureString), "%4.1f*", newTemp); // e.g. " 5.2*" draws " 5.2°C"
                xSemaphoreGive(sensorStringMutex);
            }
            valueChanged.store(true);
            Logger::printf("GY21 Temperature updated: %.2f C\n", newTemp);
        }

        if (fabs(newHumidity - humidity.load()) >= MIN_HUMIDITY_CHANGE)
        {
            prevHumidity.store(humidity.load());
            humidity.store(newHumidity);
            if (xSemaphoreTake(sensorStringMutex, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                // we've remapped the percent symbol in the font to '/' for easier display on LED matrix
                snprintf(humidityString, sizeof(humidityString), "%2.0f/", newHumidity); // e.g. "55/" draws "55%"
                xSemaphoreGive(sensorStringMutex);
            }
            valueChanged.store(true);
            Logger::printf("GY21 Humidity updated: %.2f %%\n", newHumidity);
        }
    }
    else
    {
        Logger::println("GY21 Sensor read failed!");
    }
}

// get temperature strings safely and fill provided buffer
void GY21Sensor::getTemperatureString(char *buffer, size_t bufferSize)
{
    if (xSemaphoreTake(sensorStringMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        strncpy(buffer, temperatureString, bufferSize - 1);
        buffer[bufferSize - 1] = '\0'; // ensure null termination
        xSemaphoreGive(sensorStringMutex);
    }
    else
    {
        // if mutex not acquired, return empty string
        if (bufferSize > 0)
        {
            buffer[0] = '\0';
        }
    }
}

// get humidity strings safely and fill provided buffer
void GY21Sensor::getHumidityString(char *buffer, size_t bufferSize)
{
    if (xSemaphoreTake(sensorStringMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        strncpy(buffer, humidityString, bufferSize - 1);
        buffer[bufferSize - 1] = '\0'; // ensure null termination
        xSemaphoreGive(sensorStringMutex);
    }
    else
    {
        if (bufferSize > 0)
        {
            buffer[0] = '\0';
        }
    }
}

// pause update task safely
void GY21Sensor::pause()
{
    enabled.store(false);
}

// resume update task safely
void GY21Sensor::resume()
{
    enabled.store(true);
}

GY21Sensor::~GY21Sensor()
{
    if (sensorStringMutex != NULL)
    {
        vSemaphoreDelete(sensorStringMutex);
    }
}