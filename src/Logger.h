#ifndef LOGGER_H
#define LOGGER_H
#pragma once

#include <Arduino.h>
#include <atomic>

// Logger class to handle logging to Serial in a thread-safe manner
// usage: Logger::begin(); Logger::printf("Hello %s", "world");
// usage: Logger::enable(false); disables output to serial monitor, true enables it
class Logger
{
public:
    static void begin(int baudRate = 115200)
    {
        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        Serial.begin(baudRate);
        enabled.store(true);
        xSemaphoreGive(loggerMutex);
    }

    static void enableOutput(bool enable)
    {
        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        enabled.store(enable);
        xSemaphoreGive(loggerMutex);
    }

    // printf with variable arguments
    static void printf(const char *format, ...)
    {
        if (!enabled) { return; }
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args); // Format the string

        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        Serial.print(buffer);
        xSemaphoreGive(loggerMutex);

        va_end(args);
    }
    // simple print....
    static void print(const char *buffer)
    {
        if (!enabled) { return; }
        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        Serial.print(buffer);
        xSemaphoreGive(loggerMutex);
    }
    static void print(String buffer)
    {
        if (!enabled) { return; }
        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        Serial.print(buffer);
        xSemaphoreGive(loggerMutex);
    }
    static void print(int number)
    {
        if (!enabled) { return; }
        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        Serial.print(number);
        xSemaphoreGive(loggerMutex);
    }
    static void print(float fNumber, int decPlaces)
    {
        if (!enabled) { return; }
        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        Serial.print(fNumber, decPlaces);
        xSemaphoreGive(loggerMutex);
    }
    // print with new line.....
    static void println(const char *buffer)
    {
        if (!enabled) { return; }
        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        Serial.println(buffer);
        xSemaphoreGive(loggerMutex);
    }
    static void println(String buffer)
    {
        if (!enabled) { return; }
        xSemaphoreTake(loggerMutex, portMAX_DELAY);
        Serial.println(buffer);
        xSemaphoreGive(loggerMutex);
    }

private:
    static std::atomic<bool> enabled;
    static SemaphoreHandle_t loggerMutex;
};

#endif