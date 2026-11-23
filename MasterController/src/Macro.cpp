#include <ActionMapper.h>
#include <LittleFS.h>
#include <Logger.h>
#include <EDGameVariables.h>
#include <ThreadSafeWebSerial.h>
#include <Macro.h>

Macro::~Macro()
{
    for (const auto& step : steps)
        delete(step);
    steps.clear();
}

Macro::Macro(JsonArray opcodes)
{
    if (opcodes.size() <= 8) {
        Serial.printf("Invalid opcodes size\n");
        return;
    }
    statusFlags1 = opcodes[0].as<uint32_t>();
    negStatusFlags1 = opcodes[1].as<uint32_t>();
    statusFlags2 = opcodes[2].as<uint32_t>();
    negStatusFlags2 = opcodes[3].as<uint32_t>();
    loadoutFlags1 = opcodes[4].as<uint32_t>();
    negLoadoutFlags1 = opcodes[5].as<uint32_t>();
    loadoutFlags2 = opcodes[6].as<uint32_t>();
    negLoadoutFlags2 = opcodes[7].as<uint32_t>();

    for (size_t i = 8; i < opcodes.size(); i++) {
        uint8_t opcode = opcodes[i].as<uint8_t>();
        if (opcode == MacroStepTypeOpcode::Delay) {
            //steps.push_back(make_unique<MacroStepDelay>(opcodes[i+1].as<uint32_t>()));
            Serial.printf("Opcode = delay\n");
            steps.push_back(new MacroStepDelay(opcodes[i+1].as<uint32_t>()));
            i++;
        } else if (opcode == MacroStepTypeOpcode::Key) {
            Serial.printf("Opcode = key\n");
            steps.push_back(new MacroStepKey(opcodes[i+1].as<uint8_t>(), opcodes[i+2].as<uint8_t>()));
            i+=2;
        } else if (opcode == MacroStepTypeOpcode::Action) {
            Serial.printf("Opcode = action\n");
            steps.push_back(new MacroStepAction(opcodes[i+1].as<uint8_t>(), opcodes[i+2].as<uint8_t>()));
            i+=2;
        }
    }
}

bool Macro::Execute()
{
    bool canExecute = 
        ((EDGameVariables.StatusFlags1 & this->statusFlags1) == this->statusFlags1)
        && ((EDGameVariables.StatusFlags2 & this->statusFlags2) == this->statusFlags2)
        && ((EDGameVariables.LoadoutFlags1 & this->loadoutFlags1) == this->loadoutFlags1)
        && ((EDGameVariables.LoadoutFlags2 & this->loadoutFlags2) == this->loadoutFlags2)
        && ((~EDGameVariables.StatusFlags1 & this->negStatusFlags1) == this->negStatusFlags1)
        && ((~EDGameVariables.StatusFlags2 & this->negStatusFlags2) == this->negStatusFlags2)
        && ((~EDGameVariables.LoadoutFlags1 & this->negLoadoutFlags1) == this->negLoadoutFlags1)
        && ((~EDGameVariables.LoadoutFlags2 & this->negLoadoutFlags2) == this->negLoadoutFlags2);
    if (canExecute)
        for (const auto& step : steps)
           step->Execute();
    
    return canExecute;
}

void Macro::Explain(String* s)
{
    s->concat("Status Flags 1 = ");
    s->concat(statusFlags1);
    s->concat("\nStatus Flags 2 = ");
    s->concat(statusFlags2);
    s->concat("\nLoadout Flags 1 = ");
    s->concat(loadoutFlags1);
    s->concat("\nLoadout Flags 2 = ");
    s->concat(loadoutFlags2);
    s->concat("\nNOT Status Flags 1 = ");
    s->concat(negStatusFlags1);
    s->concat("\nNOT Status Flags 2 = ");
    s->concat(negStatusFlags2);
    s->concat("\nNOT Loadout Flags 1 = ");
    s->concat(negLoadoutFlags1);
    s->concat("\nNOT Loadout Flags 2 = ");
    s->concat(negLoadoutFlags2);
    s->concat("\nActions :\n");
    for (const auto& step : steps)
        step->Explain(s);
}

bool MacroStepAction::Execute()
{
    // return false when action could not be completed, or if opcode is invalid
    if (_action == MacroStepActionOpcode::Hud) {
        if (EDGameVariables.IsInDSDMode() || EDGameVariables.IsInFSAMode())
            return false;

        if (_option == MacroStepActionHudOpcode::Analysis) {
            if (!EDGameVariables.IsHudAnalysisMode())
            {
                //TSWebSerial.println("HUD mode mismatch, switching");
                ActionMapper.SendKey(ActionMapper.GetItemConfig(COCKPIT_MODE).pressedKey, true, 1);
            }
        } else if (_option == MacroStepActionHudOpcode::Combat) {
            if (EDGameVariables.IsHudAnalysisMode())
            {
                //TSWebSerial.println("HUD mode mismatch, switching");
                ActionMapper.SendKey(ActionMapper.GetItemConfig(COCKPIT_MODE).pressedKey, true, 1);
            }
        } else {
            return false;
        }
    } else if (_action == MacroStepActionOpcode::Panel) {
        if (_option == MacroStepActionPanelOpcode::Center) {
            if (EDGameVariables.GuiFocus != (uint8_t)GUIFOCUS_MODE::GF_NOFOCUS)
            {
                // ActionMapper.SendKey(GUI_FOCUS_CENTER, true, 1);
            }
        } else if (_option == MacroStepActionPanelOpcode::Left) {
            if (EDGameVariables.GuiFocus != (uint8_t)GUIFOCUS_MODE::GF_EXTERNALPANEL)
            {
                // ActionMapper.SendKey(GUI_FOCUS_EXTERNAL, true, 1);
            }
        } else if (_option == MacroStepActionPanelOpcode::Right) {
            if (EDGameVariables.GuiFocus != (uint8_t)GUIFOCUS_MODE::GF_INTERNALPANEL)
            {
                // ActionMapper.SendKey(GUI_FOCUS_INTERNAL, true, 1);
            }
        } else {
            return false;
        }
    } else if (_action == MacroStepActionOpcode::Tab) {
        if (_option == MacroStepActionTabOpcode::Next) {
            // ActionMapper.SendKey(GUI_TAB_NEXT, true, 1);            
        } else if (_option == MacroStepActionTabOpcode::Previous) {
            // ActionMapper.SendKey(GUI_TAB_PREVIOUS, true, 1);            
        } else {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

void MacroStepAction::Explain(String *s)
{
    s->concat("\t- Action : ");
    if (_action == MacroStepActionOpcode::Hud) {
        if (_option == MacroStepActionHudOpcode::Analysis) {
            s->concat("Switch to analysis mode");
        } else if (_option == MacroStepActionHudOpcode::Combat) {
            s->concat("Switch to combat mode");
        }
    } else if (_action == MacroStepActionOpcode::Panel) {
        if (_option == MacroStepActionPanelOpcode::Center) {
            s->concat("Switch to center panel");
        } else if (_option == MacroStepActionPanelOpcode::Left) {
            s->concat("Switch to left panel (external)");
        } else if (_option == MacroStepActionPanelOpcode::Right) {
            s->concat("Switch to right panel (internal)");
        }
    } else if (_action == MacroStepActionOpcode::Tab) {
        if (_option == MacroStepActionTabOpcode::Next) {
            s->concat("Switch to next tab");
        } else if (_option == MacroStepActionTabOpcode::Previous) {
            s->concat("Switch to previous tab");
        }
    }
    s->concat("\n");
}

bool MacroStepKey::Execute()
{    
    ActionMapper.SendKey(_scanCode, true, _repeats);
    return true;
}

void MacroStepKey::Explain(String *s)
{
    s->concat("\t- Send key ");
    s->concat(_scanCode);
    s->concat(" ");
    s->concat(_repeats);
    s->concat(" times\n");
}

bool MacroStepDelay::Execute()
{
    // TODO : Send a message to slave mcu to suspend sending keys for _delay ms
    const TickType_t xDelay = _delay / portTICK_PERIOD_MS;
    vTaskDelay(xDelay);
    return true;
}

void MacroStepDelay::Explain(String *s)
{
    s->concat("\t- Delay ");
    s->concat(_delay);
    s->concat(" ms\n");
}
