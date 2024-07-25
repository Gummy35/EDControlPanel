#include <EDControllerWebServer.h>
#include <I2CDevice.h>
#include <WiFi.h>
#include <ElegantOTA.h>
#include <WebSerial.h>
#include <Logger.h>

#define WIFI_CREDENTIALS_FILE "/wifi_credentials.txt"
#define AP_SSID "ESP32_AP"

EDControllerWebServerClass::EDControllerWebServerClass()
{
  _server = new AsyncWebServer(80);
  wifi_ssid = "";
  wifi_password = "";
}

bool EDControllerWebServerClass::_readWiFiCredentials(String &ssid, String &password)
{
  File file = LittleFS.open(WIFI_CREDENTIALS_FILE, FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open wifi credentials file");
    return false;
  }

  ssid = file.readStringUntil('\n');
  password = file.readStringUntil('\n');

  // Remove newline characters
  ssid.trim();
  password.trim();

  file.close();

  return !ssid.isEmpty() && !password.isEmpty();
}

void EDControllerWebServerClass::_startAPMode()
{
  WiFi.softAP(AP_SSID);

  Serial.println("Access Point started:");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void EDControllerWebServerClass::_connectToWiFi(const String &ssid, const String &password)
{
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10)
  {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Connected to WiFi successfully.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Failed to connect to WiFi. Starting AP mode.");
    _startAPMode();
  }
}

bool EDControllerWebServerClass::_writeWiFiCredentials(const String &ssid, const String &password)
{
  File file = LittleFS.open(WIFI_CREDENTIALS_FILE, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open wifi credentials file for writing");
    return false;
  }

  file.println(ssid);
  file.println(password);

  file.close();
  return true;
}

void EDControllerWebServerClass::_onOTAEnd(bool success)
{
  // Log when OTA has finished
  if (success)
  {
    Logger.Log("OTA update complete");
    _writeWiFiCredentials(wifi_ssid, wifi_password);
    LittleFS.end();
  }
  else
  {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

void EDControllerWebServerClass::_handleSaveWiFi(AsyncWebServerRequest *request)
{
  if (request->hasArg("ssid") && request->hasArg("password"))
  {
    String ssid = request->arg("ssid");
    String password = request->arg("password");

    if (_writeWiFiCredentials(ssid, password))
    {
      request->send(200, "text/plain", "Credentials saved. Please restart the device.");
      LittleFS.end();
      ESP.restart();
    }
    else
    {
      request->send(500, "text/plain", "Failed to save credentials.");
    }
  }
  else
  {
    request->send(400, "text/plain", "Invalid request.");
  }
}

void EDControllerWebServerClass::_handleClearWiFi(AsyncWebServerRequest *request)
{
  if (LittleFS.remove(WIFI_CREDENTIALS_FILE))
  {
    WiFi.eraseAP();
    ESP.restart();
    request->send(200, "text/plain", "Credentials cleared.");
  }
  else
  {
    request->send(500, "text/plain", "Failed to clear credentials.");
  }
}

String EDControllerWebServerClass::_htmlProcessor(const String &var)
{
  if (var == "WIFI_SSID")
    return wifi_ssid;
  return String();
}

void EDControllerWebServerClass::_handleRoot(AsyncWebServerRequest *request)
{
  request->send(LittleFS, "/html/index.html", String(), false, [this](const String &str)
                { return this->_htmlProcessor(str); });
}

void EDControllerWebServerClass::begin()
{
  if (_readWiFiCredentials(wifi_ssid, wifi_password))
  {
    Serial.println("WiFi credentials read successfully.");
    Serial.printf("SSID: %s, Password: %s\n", wifi_ssid.c_str(), wifi_password.c_str());
    _connectToWiFi(wifi_ssid, wifi_password);
  }
  else
  {
    Serial.println("WiFi credentials not found or invalid. Starting AP mode.");
    _startAPMode();
  }
  _server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->_handleRoot(request); });
  _server->on("/savewifi", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->_handleSaveWiFi(request); });
  _server->on("/clearwifi", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->_handleClearWiFi(request); });
  _server->serveStatic("/static/", LittleFS, "/html/");

  ElegantOTA.onEnd([this](bool success)
                   { this->_onOTAEnd(success); });

  WebSerial.begin(_server);
  ElegantOTA.begin(_server); // Start ElegantOTA
  _server->begin();
}

void EDControllerWebServerClass::showIp()
{
  if (WiFi.getMode() == WIFI_MODE_STA)
    Logger.Log(WiFi.localIP().toString().c_str());
  else if (WiFi.getMode() == WIFI_MODE_AP)
    Logger.Log(WiFi.softAPIP().toString().c_str());
}

void EDControllerWebServerClass::loop()
{

  WebSerial.loop();
  ElegantOTA.loop();
}

EDControllerWebServerClass EDControllerWebServer;