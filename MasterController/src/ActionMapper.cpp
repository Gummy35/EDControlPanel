#include <ActionMapper.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Logger.h>
#include <EDGameVariables.h>
#include <WebSerial.h>

#define SYNC_TOLERANCE 5000

ActionMapperClass::ActionMapperClass()
{

}

void ActionMapperClass::Init()
{
    LoadDefaultMap();
    // if (!LoadMapping())
    // {
    //     LoadDefaultMap();
    //     SaveMapping();
    // }

    if (!LoadHardpoints())
    {
        LoadDefaultHardpointConfig();
        SaveHardpoints();
    }
}

/// @brief Unused at the moment
/// @return 
bool ActionMapperClass::LoadMapping() {
    File file = LittleFS.open("/actionsMap.json", "r");
    if (!file) {        
        Logger.Log("Failed to open file for reading");
        return false;
    }

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Logger.Log("Failed to read file, using default configuration");
        file.close();
        return false;
    }

    Logger.Log("Keymap config loaded");
    
    _actionsMap.clear();

    for (JsonPair kv : doc.as<JsonObject>()) {
        uint8_t key = String(kv.key().c_str()).toInt();
        JsonObject item = kv.value().as<JsonObject>();
        ActionMapperItem value;
        value.type = static_cast<ActionnerType>(item["t"].as<uint8_t>());
        value.pressedKey = item["p"].as<uint8_t>();
        value.releasedKey = item["r"].as<uint8_t>();
        value.pressCount = item["c"].as<uint8_t>();
        value.state = static_cast<ActionnerState>(item["s"].as<uint8_t>());
        _actionsMap[key] = value;
    }

    file.close();
    RebuildToggleList();
    return true;
}

/// @brief Unused at the moment
/// @return 
void ActionMapperClass::SaveMapping() {
    File file = LittleFS.open("/actionsMap.json", "w");
    if (!file) {
        Logger.Log("Failed to open file for writing");
        return;
    }

    JsonDocument doc;

    for (const auto& kv : _actionsMap) {
        JsonObject item = doc.createNestedObject(String(kv.first));
        item["t"] = kv.second.type;
        item["p"] = kv.second.pressedKey;
        item["r"] = kv.second.releasedKey;
        item["c"] = kv.second.pressCount;
        item["s"] = kv.second.state;
    }

    if (serializeJson(doc, file) == 0) {
        Logger.Log("Failed to write to file");
    }

    Logger.Log("Keymap config saved");

    file.close();
}

void ActionMapperClass::LoadDefaultMap()
{
    _actionsMap.clear();

    _actionsMap[SRV_1] = {ActionnerType::Button, 'u', 0x00, 1}; // U TurretMode
    _actionsMap[SRV_2] = {ActionnerType::Button, 'x', 0x00, 1}; // X Handbrake
    _actionsMap[SRV_3] = {ActionnerType::Button, 'r', 0x00, 1}; // X Recall/dismiss ship
    _actionsMap[SRV_4] = {ActionnerType::Button, 'z', 0x00, 1}; // Z Drive Assist

    _actionsMap[FIGHTER_ATTACK] = {ActionnerType::Button, 0xE3, 0x00, 1}; // NP3
    _actionsMap[FIGHTER_DEFEND] = {ActionnerType::Button, 0xE1, 0x00, 1}; // NP1
    _actionsMap[FIGHTER_HOLDFIRE] = {ActionnerType::Button, 0xE4, 0x00, 1}; // NP4
    _actionsMap[FIGHTER_HOLDPOS] = {ActionnerType::Button, 0xE5, 0x00, 1}; // NP5
    _actionsMap[FIGHTER_DOCK] = {ActionnerType::Button, 0xEA, 0x00, 1}; // NP0
    _actionsMap[FIGHTER_FOLLOW] = {ActionnerType::Button, 0xE6, 0x00, 1}; // NP6
    _actionsMap[FIGHTER_FIREATWILL] = {ActionnerType::Button, 0xE2, 0x00, 1}; // NP2

    _actionsMap[WING_TARGET] = {ActionnerType::Button, '0', 0x27, 1}; // 0 / à
    _actionsMap[WING_1] = {ActionnerType::Button, '7', 0x24, 1}; // 7 / è
    _actionsMap[WING_2] = {ActionnerType::Button, '8', 0x25, 1}; // 8 / _
    _actionsMap[WING_NAVLOCK] = {ActionnerType::Button, 0x2d, 0x00, 1}; // )
    _actionsMap[WING_3] = {ActionnerType::Button, '9', 0x26, 1}; // 9

    _actionsMap[STARNAV_GALAXY_MAP] = {ActionnerType::Button, 0xCB, 0x00, 1}; // F10
    _actionsMap[STARNAV_SYSTEM_MAP] = {ActionnerType::Button, 0xCC, 0x00, 1}; // F11
    _actionsMap[STARNAV_NEXT_SYSTEM] = {ActionnerType::Button, 0xCD, 0x00, 1}; // F12

    _actionsMap[DEFENSE_SHIELD_CELL] = {ActionnerType::Button, 0xC6, 0x00, 1}; // F5
    _actionsMap[DEFENSE_HEAT_SINK] = {ActionnerType::Button, 'v', 0x00, 1}; // V
    _actionsMap[DEFENSE_CHAFF] = {ActionnerType::Button, 0xC7, 0x00, 1}; // F6
    _actionsMap[DEFENSE_ECM] = {ActionnerType::Button, 0xC8, 0xC8, 0}; // F7

    _actionsMap[ENERGY_SYSTEMS] = {ActionnerType::Button, 0xD8, 0x00, 1}; // LEFT
    _actionsMap[ENERGY_ENGINE] = {ActionnerType::Button, 0xDA, 0x00, 1}; // UP
    _actionsMap[ENERGY_WEAPONS] = {ActionnerType::Button, 0xD7, 0x00, 1}; // RIGHT
    _actionsMap[ENERGY_BALANCE] = {ActionnerType::Button, 0xD9, 0x00, 1}; // DOWN

    _actionsMap[COCKPIT_MODE] = {ActionnerType::Button, 'm', 0x00, 1}; // M 

    _actionsMap[TARGETING_MOST_HOSTILE] = {ActionnerType::Button, 'c', 0x00, 1};
    _actionsMap[TARGETING_MAIN_THREAT] = {ActionnerType::Button, 'h', 0x00, 1}; // H
    _actionsMap[TARGETING_PREVIOUS_THREAT] = {ActionnerType::Button, 0xC9, 0x00, 1}; // F8
    _actionsMap[TARGETING_NEXT_THREAT] = {ActionnerType::Button, 0xCA, 0x00, 1}; //F9
    _actionsMap[TARGETING_NEXT_TARGET] = {ActionnerType::Button, 'g', 0x00, 1}; // G
    _actionsMap[TARGETING_NEXT_SUBSYS] = {ActionnerType::Button, 'y', 0x00, 1}; // Y
    _actionsMap[TARGETING_NEXT_GROUP] = {ActionnerType::Button, 'n', 0x00, 1}; // N

    _actionsMap[ENGINE_CRUISE] = {ActionnerType::Button, 'k', 0x00, 1}; // K
    _actionsMap[ENGINE_JUMP] = {ActionnerType::Button, 'i', 0x00, 1}; // I
    _actionsMap[ENGINE_DISENGAGE] = {ActionnerType::Button, 'k', 0x00, 1}; // K
    _actionsMap[ENGINE_FULL_STOP] = {ActionnerType::Button, 'x', 0x00, 1}; // X

    _actionsMap[SHIP_SILENT_RUNNING] = {ActionnerType::Toggle, 0xD4, 0xD4, 1, ActionnerState::Init}; // SUPPR
    _actionsMap[SHIP_JETTISON_CARGO] = {ActionnerType::Button, 0xDE, 0x00, 1}; // KP -
    _actionsMap[SHIP_EMERGENCY_STOP] = {ActionnerType::Button, 'k', 0x00, 2}; // 2*K
    _actionsMap[SHIP_CARGO_SCOOP] = {ActionnerType::Toggle, 0xD2, 0xD2, 1, ActionnerState::Init}; // HOME
    _actionsMap[SHIP_LIGHTS] = {ActionnerType::Toggle, 0xD1, 0xD1, 1, ActionnerState::Init}; // INSERT
    _actionsMap[SHIP_HARDPOINTS] = {ActionnerType::Toggle, 'u', 'u', 1, ActionnerState::Init}; // U
    _actionsMap[SHIP_LANDING_GEARS] = {ActionnerType::Toggle, 'l', 'l', 1, ActionnerState::Init}; // L
    _actionsMap[SHIP_FLIGHT_ASSIST] = {ActionnerType::Toggle, 'z', 'z', 1, ActionnerState::Init}; // Z

    _actionsMap[SCANNER_DSD] = {ActionnerType::Toggle, 0xC3, 0xB2, 1}; // F2 - BACKSPACE 
    _actionsMap[SCANNER_PREVIOUS_FILTER] = {ActionnerType::Button, 'q', 0x00, 1}; // Q
    _actionsMap[SCANNER_NEXT_FILTER] = {ActionnerType::Button, 'e', 0x00, 1}; // E

    _actionsMap[SCANNER_ACS] = {ActionnerType::Toggle, 0xC5, 0xB2, 1}; // F4 - BACKSPACE
    _actionsMap[SCANNER_ZOOM_LEFT] = {ActionnerType::Discrete, 0xD9, 0x00, 1}; // DOWN
    _actionsMap[SCANNER_ZOOM_RIGHT] = {ActionnerType::Discrete, 0xDA, 0x00, 1}; // UP
    _actionsMap[SCANNER_ZOOM_CLICK] = {ActionnerType::Button, 0xDD, 0x00, 1}; // KP * - Discover analysis
    _actionsMap[SCANNER_FREQ_LEFT] = {ActionnerType::Discrete, 'a', 0x00, 1}; // [
    _actionsMap[SCANNER_FREQ_RIGHT] = {ActionnerType::Discrete, 'd', 0x00, 1}; // ]
    _actionsMap[SCANNER_FREQ_TARGET] = {ActionnerType::Button, 't', 0x00, 1}; // T

    _actionsMap[TARGETING_SENSORS_LEFT] = {ActionnerType::Discrete, 0xD6, 0x00, 1}; // PG DOWN
    _actionsMap[TARGETING_SENSORS_RIGHT] = {ActionnerType::Discrete, 0xD3, 0x00, 1}; // PG UP
    // #define TARGETING_SENSORS_CLICK 82

    RebuildToggleList();
}

bool ActionMapperClass::LoadHardpoints() {
    File file = LittleFS.open("/hardpoints.json", "r");
    if (!file) {        
        Logger.Log("Failed to open file for reading");
        return false;
    }

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Logger.Log("Failed to read file, using default configuration");
        file.close();
        return false;
    }

    Logger.Log("Hardpoints config loaded");
    
    Hardpoints.clear();
    EDGameVariables.FireGroupCount = doc["groupcount"].as<uint8_t>();
    for (JsonPair kv : doc["hardpoints"].as<JsonObject>()) {
        uint8_t key = String(kv.key().c_str()).toInt();
        JsonObject item = kv.value().as<JsonObject>();
        HardpointItem value;
        value.fireGroup = item["group"].as<uint8_t>();
        value.fireWeapon = item["weapon"].as<uint8_t>();
        value.analysisMode = item["analysis"].as<uint8_t>() == 1 ? true : false;
        Hardpoints[key] = value;
    }
    serializeJson(doc, WebSerial);

    file.close();
    return true;
}

void ActionMapperClass::SaveHardpoints() {
    File file = LittleFS.open("/hardpoints.json", "w");
    if (!file) {
        Logger.Log("Failed to open file for writing");
        return;
    }

    JsonDocument doc;
    doc["groupcount"] = EDGameVariables.FireGroupCount;
    JsonObject hardpoints = doc.createNestedObject("hardpoints");
    for (const auto& kv : Hardpoints) {
        JsonObject item = hardpoints.createNestedObject(String(kv.first));
        item["group"] = kv.second.fireGroup;
        item["weapon"] = kv.second.fireWeapon;
        item["analysis"] = kv.second.analysisMode ? 1 : 0;
    }
    serializeJson(doc, WebSerial);
    if (serializeJson(doc, file) == 0) {
        Logger.Log("Failed to write to file");
    }
    
    file.close();
}

void ActionMapperClass::LoadDefaultHardpointConfig()
{
    Hardpoints.clear();
    EDGameVariables.FireGroupCount = 3;
    Hardpoints[SCANNER_DSD] = {1, 1, true};
}

void ActionMapperClass::SetItemConfig(uint8_t item, ActionMapperItem itemConfig)
{
    _actionsMap[item] = itemConfig;
}

ActionMapperItem ActionMapperClass::GetItemConfig(uint8_t item)
{
    return _actionsMap[item];
}

void ActionMapperClass::TriggerActionItem(uint8_t item, bool pressed, uint8_t count)
{
    Serial.printf("TAI received %d %d %d\n", item, pressed, count);
    // if no callback is specified, just exit
    if (_sendKey == nullptr) return;
    // retrieve action parameters
    ActionMapperItem itemConfig = _actionsMap[item];
    Serial.printf("Mapped into %d\n", itemConfig.pressedKey);

    // if action is mapped as a momentary button/switch,
    if (itemConfig.type == ActionnerType::Button) {
        // if pressed, use the key pressed config
        if (pressed)
            _sendKey(itemConfig.pressedKey, true, itemConfig.pressCount);
        else
        // if released, send only one release keystroke
            _sendKey(itemConfig.releasedKey, false, itemConfig.pressCount);
    } else if (itemConfig.type == ActionnerType::Discrete) {
        // if actionner is a discrete type (rotary button for example), 
        // send as many press count as specified by config AND call
        _sendKey(itemConfig.pressedKey, true, itemConfig.pressCount * count);        
    } else if (itemConfig.type == ActionnerType::Toggle) {     
        if (!_toggles[item]->isInconsistent())
        {
            _toggleAction(item, &itemConfig, pressed, count);
            _toggles[item]->sentTS = millis();
        }
        if (pressed) {
            _toggles[item]->localState = ActionnerState::Active;
        } else {
            _toggles[item]->localState = ActionnerState::Inactive;    
        }        
    }   
}

std::vector<uint8_t> ActionMapperClass::GetInconsistencies()
{
    _inconsistentToggles.clear();

    for (const auto& kv : _toggles) {
        if (kv.second->isInconsistent())
        {
            Serial.printf("Key %d inconsistent, adding to inconsistencies", kv.first);
            _inconsistentToggles.push_back(kv.first);
        }
    }

    return _inconsistentToggles;
}

void ActionMapperClass::RebuildToggleList()
{
    for (const auto& kv : _toggles) {
        delete kv.second;
    }
    
    _toggles.clear();

    for (const auto& kv : _actionsMap) {
        if (kv.second.type == ActionnerType::Toggle)
            _toggles[kv.first] = new ToggleButton();
    }
}

void ActionMapperClass::ClearInconsistencies()
{
    _inconsistentToggles.clear();
}

void ActionMapperClass::UpdateRemoteStatus()
{
    unsigned long now = millis();
    for (const auto& kv : _toggles) {
        kv.second->syncTS = now;
        kv.second->remoteState = _getGameStatus(kv.first);
        Serial.printf("new remote status for %d: %d\n", kv.first, kv.second->remoteState);
    }
}


void ActionMapperClass::UpdateLocalStatus(uint8_t item, bool physicalStatus)
{
    _toggles[item]->localState = physicalStatus ? ActionnerState::Active : ActionnerState::Inactive;
}

void ActionMapperClass::TriggerHardpoint(uint8_t item)
{
    if (Hardpoints.find(item) != Hardpoints.end()) {
        WebSerial.printf("Hardpoint %d found\n");
        HardpointItem hardpoint = Hardpoints[item];
        // switch cockpit mode if necessary
        if (hardpoint.analysisMode != EDGameVariables.IsHudAnalysisMode())
        {
            WebSerial.println("HUD mode mismatch, switching");
            ActionMapper.SendKey(ActionMapper.GetItemConfig(COCKPIT_MODE).pressedKey, true, 1);
        } 
        else 
        {
            WebSerial.println("HUD mode looks good");
        }
        // Switch to the proper firegroup
        uint8_t n = 0;
        if (hardpoint.fireGroup != EDGameVariables.FireGroup) {
            if (hardpoint.fireGroup > EDGameVariables.FireGroup)
                n = hardpoint.fireGroup - EDGameVariables.FireGroup;
            else 
                n = (EDGameVariables.FireGroupCount - EDGameVariables.FireGroup) + hardpoint.fireGroup;
            if (n > 0)
            {
                WebSerial.printf("firegroup mismatch, current=%d, target=%d, total=%d, sending %d switches\n", EDGameVariables.FireGroup, hardpoint.fireGroup, EDGameVariables.FireGroupCount, n);
                ActionMapper.SendKey(ActionMapper.GetItemConfig(TARGETING_NEXT_GROUP).pressedKey, true, n);
                delay(200);
            }
        }

        // Trigger hardpoint weapon
        WebSerial.printf("Firing %s weapon\n", hardpoint.fireWeapon == 1 ? "Primary" : "Secondary");
        ActionMapper.SendKey(hardpoint.fireWeapon == 1 ? FIRE_WEAPON1 : FIRE_WEAPON2, true, 1);
    }
}

void ActionMapperClass::registerToggleActionHandler(std::function<void(uint8_t item, ActionMapperItem* itemConfig, bool pressed, u_int8_t count)> handler)
{
    _toggleAction = handler;
}

void ActionMapperClass::registerGetGameStatusHandler(std::function<ActionnerState(uint8_t item)> handler)
{
    _getGameStatus = handler;
}

void ActionMapperClass::registerSetGameStatusHandler(std::function<void(uint8_t item, ActionnerState status)> handler)
{
    _setGameStatus = handler;
}

void ActionMapperClass::registerSendKeyHandler(std::function<void(uint8_t keyCode, bool pressed, u_int8_t count)> handler)
{
    _sendKey = handler;
}

void ActionMapperClass::SendKey(uint8_t keyCode, bool pressed, u_int8_t count)
{
    _sendKey(keyCode, pressed, count);
}

ActionMapperClass ActionMapper;

ToggleButton::ToggleButton()
{
}

bool ToggleButton::isInconsistent()
{
    if ((remoteState != ActionnerState::Init) && (localState != ActionnerState::Init) && (localState != remoteState))
    {
        Serial.print("Toggle is not in expected state :");
        unsigned long now = millis();
        bool isOldRemote = (now > syncTS) && ((now - syncTS) > SYNC_TOLERANCE); // inconsistent, remote update older than SYNC_TOLERANCE ms
        bool isOldLocal = (now > sentTS) && ((now - sentTS) > SYNC_TOLERANCE); // inconsistent, local update older than SYNC_TOLERANCE ms
        bool isInvalid = (syncTS > sentTS) && ((syncTS - sentTS) > SYNC_TOLERANCE); // inconsistent and last sent key ignored by remote update for longer than SYNC_TOLERANCE ms
        if (isOldRemote) Serial.printf("Old remote");
        if (isOldLocal) Serial.print("Old local");
        if (isInvalid) Serial.print("Invalid");
        Serial.println();
        return isInvalid || (isOldLocal && isOldRemote); // key has been ignored or no local and no remote update for longer than SYNC_TOLERANCE ms
    }
    return false;
}
