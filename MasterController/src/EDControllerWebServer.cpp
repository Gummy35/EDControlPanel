#include <EDControllerWebServer.h>
#include <I2CDevice.h>
#include <WiFi.h>
#include <ElegantOTA.h>
#include <ThreadSafeWebSerial.h>
#include <Logger.h>
#include <ActionMapper.h>
#include <EDGameVariables.h>

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
    ActionMapper.SaveHardpoints();
    ActionMapper.SaveMapping(MAPPING_CONFIG_FILE);
    LittleFS.end();
  }
  else
  {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

void EDControllerWebServerClass::_handleGetMacros(AsyncWebServerRequest *request)
{
  if (LittleFS.exists(MACROS_CONFIG_FILE))
    request->send(LittleFS, MACROS_CONFIG_FILE, "application/json");
  else
    request->send(200, "application/json", "{}");
}

void EDControllerWebServerClass::_handleGetJsonFile(AsyncWebServerRequest *request, const char *filename)
{
  if (LittleFS.exists(filename))
    request->send(LittleFS, filename, "application/json");
  else
    request->send(200, "application/json", "{}");
}

void EDControllerWebServerClass::_saveJsonToFile(AsyncWebServerRequest *request, JsonVariant &json, const char *filename)
{
  JsonDocument data;
  if (json.is<JsonArray>())
  {
    data = json.as<JsonArray>();
  }
  else if (json.is<JsonObject>())
  {
    data = json.as<JsonObject>();
  }

  File file = LittleFS.open(filename, "w");
  if (!file)
  {
    Logger.Log("Failed to open file for writing");
    request->send(500, "application/json", "{error:\"Failed to open file for writing\"}");
    return;
  }

  if (serializeJson(data, file) == 0)
  {
    Logger.Log("Failed to write to file");
    request->send(500, "application/json", "{error:\"Failed to write to file\"}");
    file.close();
    return;
  }

  file.close();
  // String response;
  // serializeJson(data, response);
  // request->send(200, "application/json", response);
  // Serial.println(response);
  Logger.Log("Config saved. Restarting...");
  request->send(200, "text/plain", "file saved");
  LittleFS.end();
  ESP.restart();
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

void EDControllerWebServerClass::_handleSaveHardpoints(AsyncWebServerRequest *request)
{
  if (request->hasArg("fire_group_count"))
    EDGameVariables.FireGroupCount = request->arg("fire_group_count").toInt();

  if (request->hasArg("hardpoint_id"))
  {
    uint8_t id = request->arg("hardpoint_id").toInt();
    uint8_t fireGroup = request->arg("fire_group").toInt() - 1;
    uint8_t fireWeapon = request->arg("fire_weapon").toInt();
    bool analysisMode = request->hasArg("analysis_mode") ? (request->arg("analysis_mode").toInt() == 1) : false;
    // No validation, but why would you want to crash your game device ?????
    ActionMapper.Hardpoints[id] = HardpointItem(fireGroup, fireWeapon, analysisMode);
  }

  ActionMapper.SaveHardpoints();
  request->send(200, "text/plain", "Configuration saved.");
}

void EDControllerWebServerClass::_handleDeleteHardpoint(AsyncWebServerRequest *request)
{
  if (request->arg("id"))
  {
    uint8_t id = request->arg("id").toInt();
    if (ActionMapper.Hardpoints.find(id) != ActionMapper.Hardpoints.end())
    {
      ActionMapper.Hardpoints.erase(id);
      ActionMapper.SaveHardpoints();
      request->send(200, "text/plain", "Configuration saved.");
      return;
    }
  }
  request->send(400, "text/plain", "Invalid request.");
}

String generateHardpointsTable(const std::map<uint8_t, HardpointItem> &hardpoints)
{
  String html;

  for (const auto &entry : hardpoints)
  {
    uint8_t id = entry.first;
    const HardpointItem &item = entry.second;

    html += "<div class='hardpoint-row'>";
    html += "<div>";
    if (id == SCANNER_DSD)
      html += "DSD Scanner";
    else
      html += String(id);
    html += "</div>";
    html += "<div>" + String(item.fireGroup + 1) + "</div>";
    html += "<div>" + String(item.fireWeapon == 1 ? "Primary" : "Secondary") + "</div>";
    html += "<div>" + String(item.analysisMode ? "Analysis" : "Combat") + "</div>";
    html += "<div></div>";
    html += "</div>";

    // html += "<div class='hardpoint-row'>";
    // html += "<div>";
    //  if (id == SCANNER_DSD)
    //   html += "DSD Scanner";
    // else
    //   html += String(id);
    // html += "</div>";
    // html += "<div><input type='number' name='fire_group' value='" + String(item.fireGroup) + "' min='0' max='255'></div>";
    // html += "<div><input type='number' name='fire_weapon' value='" + String(item.fireWeapon) + "' min='0' max='255'></div>";
    // html += "<div><select name='analysis_mode'><option value='0'" + String(item.analysisMode ? "" : " selected") + ">No</option><option value='1'" + (item.analysisMode ? " selected" : "") + ">Yes</option></select></div>";
    // html += "<div><button type='submit'>Save</button></div>";
    // html += "</div>";
  }

  return html;
}

String EDControllerWebServerClass::_htmlProcessor(const String &var)
{
  if (var == "WIFI_SSID")
    return wifi_ssid;
  if (var == "FIRE_GROUP_COUNT")
    return String(EDGameVariables.FireGroupCount);
  if (var == "HARDPOINTS")
    return generateHardpointsTable(ActionMapper.Hardpoints);

  return String();
}

void EDControllerWebServerClass::_handlePage(AsyncWebServerRequest *request, const char *pagePath)
{
  request->send(LittleFS, pagePath, String(), false, [this](const String &str)
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
              { this->_handlePage(request, "/html/index.html"); });
  _server->on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->_handlePage(request, "/html/wifi.html"); });
  _server->on("/hardpoints", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->_handlePage(request, "/html/hardpoints.html"); });
  _server->on("/macros", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->_handlePage(request, "/html/macros.html"); });
  _server->on("/mapping", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->_handlePage(request, "/html/mapping.html"); });
  _server->on("/savewifi", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->_handleSaveWiFi(request); });
  _server->on("/clearwifi", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->_handleClearWiFi(request); });
  _server->on("/savehardpoints", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->_handleSaveHardpoints(request); });
  _server->on("/deletehardpoint", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->_handleDeleteHardpoint(request); });
  _server->on("/getmacros", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->_handleGetJsonFile(request, MACROS_CONFIG_FILE); });
  _server->addHandler(new AsyncCallbackJsonWebHandler("/savemacros", [this](AsyncWebServerRequest *request, JsonVariant &json)
                                                      { this->_saveJsonToFile(request, json, MACROS_CONFIG_FILE); }));
  _server->on("/getmapping", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->_handleGetJsonFile(request, MAPPING_CONFIG_FILE); });
  _server->on("/getdefaultmapping", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->_handleGetJsonFile(request, MAPPING_DEFAULT_CONFIG_FILE); });
  _server->addHandler(new AsyncCallbackJsonWebHandler("/savemapping", [this](AsyncWebServerRequest *request, JsonVariant &json)
                                                      { this->_saveJsonToFile(request, json, MAPPING_CONFIG_FILE); }));

  _server->serveStatic("/static/", LittleFS, "/html/");

  ElegantOTA.onEnd([this](bool success)
                   { this->_onOTAEnd(success); });

  TSWebSerial.begin(_server);
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
  TSWebSerial.loop();
  ElegantOTA.loop();
}

EDControllerWebServerClass EDControllerWebServer;