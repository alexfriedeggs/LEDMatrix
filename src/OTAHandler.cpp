#include "OTAHandler.h"

OTAHandler::OTAHandler(const char *ssid, const char *password,
                       const char *deviceName, unsigned long reconnectIntervalMs,
                       unsigned long connectTimeoutMS)
{
    // trim to 31 chars + 1 null terminator
    // used in platformio.ini with '.local' suffix, e.g upload_port = ESP32OTADEVICE.local
    snprintf(this->hostname, sizeof(this->hostname), "%.31s", deviceName);

    // safely store ssid and password
    strncpy(this->ssid, ssid, SSID_MAX_LEN - 1);
    this->ssid[SSID_MAX_LEN - 1] = '\0';
    strncpy(this->password, password, PASS_MAX_LEN - 1);
    this->password[PASS_MAX_LEN - 1] = '\0';

    this->reconnectIntervalMS = reconnectIntervalMs;
    this->connectTimeoutMS = connectTimeoutMS;

    WiFi.persistent(false);      // don’t write creds to flash repeatedly
    WiFi.setAutoReconnect(true); // let stack auto-recover
    WiFi.setHostname(hostname);
    WiFi.mode(WIFI_STA);

    Logger::printf("Connecting to WiFi...%s\n", this->ssid);

    // attempt initial connection and delay for up to connectTimeoutMS milliseconds
    // or until connected
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < connectTimeoutMS)
    {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Logger::println("WiFi connected.");
        Logger::printf("Hostname: %s\n", hostname);
        Logger::printf("Device IP: %s\n", WiFi.localIP().toString().c_str());

        prevWifiStatus = WL_CONNECTED; // prevents reconnect logic in first handle() call

        // Start mDNS responder (name service for .local addressing)
        if (!MDNS.begin(hostname))
            Logger::println("Error setting up mDNS responder!");
        else
            Logger::printf("mDNS responder started: %s.local\n", hostname);
    }
    else
    {
        Logger::printf("WiFi not connected. Will retry every %lu seconds in background.",
                       reconnectIntervalMS / 1000);
    }

    // Setup OTA handlers for OTA events
    ArduinoOTA.onStart([]()
                       {
        Logger::println("OTA Update Start");
        Logger::printf("Hostname: %s\n", WiFi.getHostname());
        Logger::printf("Device IP: %s\n", WiFi.localIP().toString().c_str()); });
    ArduinoOTA.onEnd([]()
                     { Logger::println("OTA Update End"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { 
                            static int counter = 0;
                            if (counter++ % 32 == 0) // log every 32nd call to reduce spam
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
    ArduinoOTA.setHostname(hostname);   // vital for mDNS resolution during OTA updates
    ArduinoOTA.begin();
}

// check is disconnected and attempt reconnect if needed but only once every
// reconnectIntervalMS milliseconds to be unobtrusive
void OTAHandler::reconnectIfNeeded()
{
    wl_status_t s = WiFi.status();

    if (s != WL_CONNECTED)
    {
        // attempt to reconnect if interval elapsed
        unsigned long now = millis();
        if (now - lastReconnectAttempt > reconnectIntervalMS)
        {
            Logger::println("WiFi lost. Attempting reconnect...");
            WiFi.disconnect(true); // true = wipe old config in some stacks
            WiFi.begin(ssid, password);
            lastReconnectAttempt = now;
        }
    }

    // Detect transition to connected
    if (s == WL_CONNECTED && prevWifiStatus != WL_CONNECTED)
    {
        Logger::printf("WiFi reconnected to %s.\n", ssid);
        Logger::printf("Hostname: %s\n", hostname);
        Logger::printf("Device IP: %s\n", WiFi.localIP().toString().c_str());

        // (Re)start mDNS
        MDNS.end();
        if (!MDNS.begin(hostname))
            Logger::println("Error setting up mDNS responder!");
        else
            Logger::printf("mDNS responder started: %s.local\n", hostname);
    }

    prevWifiStatus = s;
}

OTAHandler::~OTAHandler()
{
    WiFi.disconnect();
    MDNS.end();
}