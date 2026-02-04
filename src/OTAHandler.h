#ifndef OTAHANDLER_H
#define OTAHANDLER_H

#pragma once

#include <ArduinoOTA.h>
#include <WiFi.h>
#include "Logger.h"

// simple class to handle OTA updates and WiFi reconnection
// usage:
// 1. create an instance of OTAHandler in your main application code
//    OTAHandler otaHandler("your-ssid", "your-password");
// 2. call otaHandler.handle() regularly in your main loop()
class OTAHandler
{
public:
    OTAHandler(const char* ssid, const char* password, unsigned long reconnectIntervalMs = 30000);
    ~OTAHandler();

    void handle()
    {
        ArduinoOTA.handle();
        reconnectIfNeeded();
    }

private:
    static constexpr size_t SSID_MAX_LEN = 33;      // 32 + 1 for null
    static constexpr size_t PASS_MAX_LEN = 65;      // 64 + 1 for null
    char ssid[SSID_MAX_LEN];
    char password[PASS_MAX_LEN];
    unsigned long lastReconnectAttempt = 0;
    unsigned long reconnectIntervalMS = 30000; // ms = 30 seconds

    void reconnectIfNeeded();
};

#endif