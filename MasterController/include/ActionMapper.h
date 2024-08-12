#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include <PanelActions.h>

#define FIRE_WEAPON1 0xE8
#define FIRE_WEAPON2 0xE9

enum ActionnerType : uint8_t
{
    Button = 0,
    Toggle = 1,
    Discrete = 2
};

enum ActionnerState : uint8_t
{
    Init = 0,
    WaitingUpdate = 1,
    Active = 2,
    Inactive = 3
};

struct ActionMapperItem
{
    ActionnerType type = ActionnerType::Button;
    uint8_t pressedKey = 0x00;
    uint8_t releasedKey = 0x00;
    uint8_t pressCount = 1;
    ActionnerState state = ActionnerState::Inactive;

    ActionMapperItem() {}
    ActionMapperItem(ActionnerType t, uint8_t pk, uint8_t rk, uint8_t pc)
        : type(t), pressedKey(pk), releasedKey(rk), pressCount(pc) {}
    ActionMapperItem(ActionnerType t, uint8_t pk, uint8_t rk, uint8_t pc, ActionnerState as)
        : type(t), pressedKey(pk), releasedKey(rk), pressCount(pc), state(as) {}
};

class ToggleButton
{
public:
    ActionnerState localState = ActionnerState::Init;
    ActionnerState remoteState = ActionnerState::Init;
    unsigned long syncTS = 0;
    unsigned long sentTS = 0;
    ToggleButton();
    bool isInconsistent();
};

class ActionMapperClass
{
public:
    ActionMapperClass();
    void Init();
    bool Load();
    void Save();
    void LoadDefaultMap();
    void SetItemConfig(uint8_t item, ActionMapperItem itemConfig);
    ActionMapperItem GetItemConfig(uint8_t item);
    void TriggerActionItem(uint8_t item, bool pressed, uint8_t count);
    std::vector<uint8_t> GetInconsistencies();
    void ClearInconsistencies();
    void RebuildToggleList();
    void UpdateRemoteStatus();
    void SendKey(uint8_t keyCode, bool pressed, u_int8_t count);
    void UpdateLocalStatus(uint8_t item, bool physicalStatus);

    void registerToggleActionHandler(std::function<void(uint8_t item, ActionMapperItem *itemConfig, bool pressed, u_int8_t count)> handler);
    void registerGetGameStatusHandler(std::function<ActionnerState(uint8_t item)> handler);
    void registerSetGameStatusHandler(std::function<void(uint8_t item, ActionnerState status)> handler);
    void registerSendKeyHandler(std::function<void(uint8_t keyCode, bool pressed, u_int8_t count)> handler);
private:
    std::map<uint8_t, ActionMapperItem> _actionsMap;
    std::map<uint8_t, ToggleButton*> _toggles;
    std::vector<uint8_t> _inconsistentToggles;
    std::function<ActionnerState(uint8_t item)> _getGameStatus = nullptr;
    std::function<void(uint8_t item, ActionnerState status)> _setGameStatus = nullptr;
    std::function<void(uint8_t keyCode, bool pressed, u_int8_t count)> _sendKey = nullptr;
    std::function<void(uint8_t item, ActionMapperItem *itemConfig, bool pressed, u_int8_t count)> _toggleAction = nullptr;
};

extern ActionMapperClass ActionMapper;
