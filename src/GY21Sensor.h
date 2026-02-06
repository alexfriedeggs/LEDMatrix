#ifndef GY21_H
#define GY21_H

#pragma once

#include "Wire.h"
#include "SHT2x.h"
#include <Arduino.h>
#include <atomic>
#include "Logger.h"

#define CALIBRATION_OFFSET_TEMP -1.0f // empirically determined offset to calibrate temperature readings

// I2C interface for GY-21 temp/humidity sensor module (SHT21/Si7021)
// Usage:
//     GY21Sensor gy21(SDA_PIN, SCL_PIN, UPDATE_INTERVAL_MS);
//     gy21.resume();  // start background updates
//     float temperature = gy21.getTemp();
//     float humidity = gy21.getHumidity();
//     getTemperatureString(buffer, bufferSize);
//     getHumidityString(buffer, bufferSize);
class GY21Sensor
{
public:
    GY21Sensor(int sda, int scl, int updateIntervalMS = 2000);
    ~GY21Sensor();

    // thread-safe getters for temperature and humidity
    float getTemp() { return temperature.load(); }
    float getHumidity() { return humidity.load(); }

    // has value changed since last read?. then reset flag
    bool hasValueChanged() { return valueChanged.exchange(false); }

    // get temperature and humidity strings safely to provided buffers
    void getTemperatureString(char *buffer, size_t bufferSize);
    void getHumidityString(char *buffer, size_t bufferSize);

    // start and stop the background update task
    void resume();
    void pause();

private:
    SHT2x gy21;

    int updateIntervalMS; // polling interval in milliseconds

    std::atomic<float> temperature;
    std::atomic<float> humidity;
    std::atomic<float> prevTemp;
    std::atomic<float> prevHumidity;
    std::atomic<bool> valueChanged;

    const float MIN_TEMP_CHANGE = 0.1f;         // minimum change in temperature to register as updated
    const float MIN_HUMIDITY_CHANGE = 1.0f;     // minimum change in humidity to register as updated

    char temperatureString[16];
    char humidityString[16];

    // read sensor and update values
    void readSensor();

    std::atomic<bool> enabled;            // controls whether the update task is running
    SemaphoreHandle_t sensorStringMutex;  // Protects all text-related members
    TaskHandle_t updateTaskHandle = NULL; // The TaskHandle for the update function

    // the main update task function that reads the sensor periodically
    void updateTask();
    // a static function wrapper we can use as a task function
    static void updateTaskWrapper(void *params)
    {
        static_cast<GY21Sensor *>(params)->updateTask();
    }
};

#endif
