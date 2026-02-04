#include "OTAHandler.h"

OTAHandler::OTAHandler(const char *ssid, const char *password, unsigned long reconnectIntervalMs)
{
    // safely store ssid and password
    strncpy(this->ssid, ssid, SSID_MAX_LEN - 1);
    this->ssid[SSID_MAX_LEN - 1] = '\0';
    strncpy(this->password, password, PASS_MAX_LEN - 1);
    this->password[PASS_MAX_LEN - 1] = '\0';
    this->reconnectIntervalMS = reconnectIntervalMs;

    WiFi.mode(WIFI_STA);
    WiFi.begin(this->ssid, this->password);
    Logger::printf("Connecting to WiFi...%s, %s\n", this->ssid, this->password);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000)
    {
        delay(100);
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Logger::println("WiFi connected.");
        Logger::printf("Device IP: %s\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        Logger::println("WiFi not connected. Will retry in background.");
    }

    ArduinoOTA.onStart([]()
                       {
        Logger::println("OTA Update Start");
        Logger::printf("Device IP: %s\n", WiFi.localIP().toString().c_str()); });
    ArduinoOTA.onEnd([]()
                     { Logger::println("OTA Update End"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { 
                            static int counter = 0;
                            if (counter++ % 10 == 0) // log every 10th call to reduce spam
                            {
                                Logger::printf("OTA Progress: %u%%\r", (progress * 100) / total);
                                counter = 0;
                            } });
    ArduinoOTA.onError([](ota_error_t error)
                       {
        Logger::printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Logger::println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Logger::println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Logger::println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Logger::println("Receive Failed");
        else if (error == OTA_END_ERROR) Logger::println("End Failed"); });
    ArduinoOTA.begin();
}

void OTAHandler::reconnectIfNeeded()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > reconnectIntervalMS)
        {
            Logger::println("WiFi lost. Attempting reconnect...");
            WiFi.disconnect();
            WiFi.begin(ssid, password);
            lastReconnectAttempt = now;
        }
    }
}

OTAHandler::~OTAHandler()
{
}