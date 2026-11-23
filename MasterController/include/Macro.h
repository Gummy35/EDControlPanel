#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include <memory>
#include <ArduinoJson.h>

// template<typename T, typename... Args>
// std::unique_ptr<T> make_unique(Args&&... args) {
//     return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
// }

enum MacroStepTypeOpcode : uint8_t
{
    Delay = 1,
    Key = 2,
    Action = 3
};

enum MacroStepActionOpcode : uint8_t
{
    Hud = 1,
    Panel = 2,
    Tab = 3
};

enum MacroStepActionHudOpcode : uint8_t
{
    Combat = 1,
    Analysis = 2,
};

enum MacroStepActionPanelOpcode : uint8_t
{
    Left = 1,
    Right = 2,
    Center = 3
};

enum MacroStepActionTabOpcode : uint8_t
{
    Next = 1,
    Previous = 2,
};



class MacroStep
{
public:
    MacroStep() {}
    virtual ~MacroStep() = default;
    virtual bool Execute() { return false; }
    virtual void Explain(String *s) {};
};

class MacroStepDelay : public MacroStep
{
public:
    MacroStepDelay(uint32_t delay):
        _delay(delay) {}
    bool Execute() override;
    void Explain(String *s) override;

private:
    uint32_t _delay;
};

class MacroStepKey : public MacroStep
{
public:
    MacroStepKey(uint8_t scanCode, uint8_t repeats):
        _scanCode(scanCode), _repeats(repeats) {}
    bool Execute() override;
    void Explain(String *s) override;

private:
    uint8_t _scanCode;
    uint8_t _repeats;
};


class MacroStepAction : public MacroStep
{
public:
    MacroStepAction(uint8_t action, uint8_t option):
        _action(action), _option(option) {}
    bool Execute() override;
    void Explain(String *s) override;

private:
    uint8_t _action;
    uint8_t _option;
};

class Macro
{
public:
    Macro() {}
    ~Macro();

    Macro(JsonArray opcodes);
    bool Execute();
    void Explain(String *s);

private:
    // default to all flags beeing true and false, making it bailout if instanciation was not successful.
    uint32_t statusFlags1 = 0xFFFFFFFF;
    uint32_t negStatusFlags1 = 0xFFFFFFFF;
    uint32_t statusFlags2 = 0xFFFFFFFF;
    uint32_t negStatusFlags2 = 0xFFFFFFFF;
    uint32_t loadoutFlags1 = 0xFFFFFFFF;
    uint32_t negLoadoutFlags1 = 0xFFFFFFFF;
    uint32_t loadoutFlags2 = 0xFFFFFFFF;
    uint32_t negLoadoutFlags2 = 0xFFFFFFFF;

    //std::vector<std::unique_ptr<MacroStep>> steps = std::vector<std::unique_ptr<MacroStep>>();
    std::vector<MacroStep*> steps = std::vector<MacroStep*>();
};