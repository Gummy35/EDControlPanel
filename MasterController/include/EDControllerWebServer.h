#pragma once

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

class EDControllerWebServerClass
{
public:
    EDControllerWebServerClass();
    void begin();
    void loop();
    void showIp();

private:
    bool _readWiFiCredentials(String &ssid, String &password);
    void _startAPMode();
    void _connectToWiFi(const String &ssid, const String &password);
    bool _writeWiFiCredentials(const String &ssid, const String &password);
    void _onOTAEnd(bool success);
    void _handleSaveWiFi(AsyncWebServerRequest *request);
    void _handleSaveHardpoints(AsyncWebServerRequest *request);
    void _handleDeleteHardpoint(AsyncWebServerRequest *request);
    void _handleClearWiFi(AsyncWebServerRequest *request);
    void _handleRoot(AsyncWebServerRequest *request);
    String _htmlProcessor(const String &var);

    String wifi_ssid;
    String wifi_password;
    AsyncWebServer *_server;
};

extern EDControllerWebServerClass EDControllerWebServer;