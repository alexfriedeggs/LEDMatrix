#ifndef OTAHANDLER_H
#define OTAHANDLER_H

#pragma once

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "Logger.h"

// simple class to handle OTA updates and WiFi reconnection
// - On Windows, Bonjour service must be installed and running for mDNS resolution.
// - UDP port 5353 must be open on your PC and router for mDNS traffic.
// - Call handle() regularly in your main loop to process OTA requests and reconnect WiFi.
// Usage:
// 1. create an instance of OTAHandler in your main application code
//      ssid: wifi name, max 32 chars
//      password: wifi password, max 64 chars
//      device-name: desired host name on DNS server, max 31 chars
//      reconnectIntervalMs: interval between reconnection attempts if wifi drops
//      connectTimeoutMS: timeout for initial connection attempt
// 2. call handle() regularly in your main loop() to handle OTS and wifi reconnects
// 3. to upload new firmware via OTA, in platformio.ini, use the espota upload protocol
//    in an extended environemtment, and choose this envrionement in VSCode for upload, e.g:
//    [env:esp32-ota]
//      extends = env:esp32-usb
//      upload_protocol = espota
//      upload_port = DEVICENAME.local
//      ;upload_port = 192.168.1.117 (backup IP)
//      (can replace DEVICENAME.local with actual IP if mDNS issues occur).#
//      (note that the .local suffix is required for mDNS resolution).
class OTAHandler
{
public:
    OTAHandler(const char *ssid, const char *password,
               const char *deviceName, unsigned long reconnectIntervalMs = 30000,
               unsigned long connectTimeoutMS = 5000);
    ~OTAHandler();

    // call once per main loop iteration to handle OTA and reconnect if needed
    void handle()
    {
        ArduinoOTA.handle();
        reconnectIfNeeded();
    }

private:
    static constexpr size_t SSID_MAX_LEN = 33; // 32 + 1 for null
    static constexpr size_t PASS_MAX_LEN = 65; // 64 + 1 for null
    char ssid[SSID_MAX_LEN];
    char password[PASS_MAX_LEN];
    char hostname[32]; // the device hostname on the DNS server

    unsigned long connectTimeoutMS;    // timeout for initial connection attempt
    unsigned long reconnectIntervalMS; // interval between reconnection attempts
    unsigned long lastReconnectAttempt = 0;

    wl_status_t prevWifiStatus = WL_IDLE_STATUS; // previous WiFi status

    // check is disconnected and attempt reconnect if needed but only once every
    // reconnectIntervalMS milliseconds to be unobtrusive
    void reconnectIfNeeded();
};

#endif