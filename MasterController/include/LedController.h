#pragma once

#include <ShiftRegisterTPIC6B595.h>
#include <PanelActions.h>

const int numberOfShiftRegisters = 6; // number of shift registers attached in series

const uint8_t LEDS_SRV[] = {SRV_1, SRV_2, SRV_3, SRV_4};
const uint8_t LEDS_FIGHTERS[] = {FIGHTER_ATTACK, FIGHTER_DEFEND, FIGHTER_DOCK, FIGHTER_FIREATWILL, FIGHTER_FOLLOW, FIGHTER_HOLDFIRE, FIGHTER_HOLDPOS};
const uint8_t LEDS_WING[] = {WING_1, WING_2, WING_3, WING_NAVLOCK, WING_TARGET};
const uint8_t LEDS_STARNAV[] = {STARNAV_GALAXY_MAP, STARNAV_NEXT_SYSTEM, STARNAV_SYSTEM_MAP, COCKPIT_MODE};
const uint8_t LEDS_DEFENSE[] = {DEFENSE_CHAFF, DEFENSE_ECM, DEFENSE_HEAT_SINK, DEFENSE_SHIELD_CELL};
const uint8_t LEDS_ENERGY[] = {ENERGY_BALANCE, ENERGY_ENGINE, ENERGY_SYSTEMS, ENERGY_WEAPONS};
const uint8_t LEDS_SCANNER[] = {SCANNER_NEXT_FILTER, SCANNER_PREVIOUS_FILTER};
const uint8_t LEDS_TARGETING[] = {TARGETING_MAIN_THREAT, TARGETING_MOST_HOSTILE, TARGETING_NEXT_GROUP, TARGETING_NEXT_SUBSYS, TARGETING_NEXT_TARGET, TARGETING_NEXT_THREAT, TARGETING_PREVIOUS_THREAT};
const uint8_t LEDS_ENGINE[] = {ENGINE_CRUISE, ENGINE_DISENGAGE, ENGINE_FULL_STOP, ENGINE_JUMP};

#define LedGroupOn(group) setLedGroup(group, sizeof(group), HIGH)
#define LedGroupOff(group) setLedGroup(group, sizeof(group), LOW)

class LedControllerClass
{
public:
    void Init(int rck, int srck, int serin);
    void SetGroup(const uint8_t *Ledgroup, int size, uint8_t value);
    void SetAllOn();
    void SetAllOff();
    void Test();

private:    
    int _serialDataPin; // = TPICSERIN;
    int _clockPin; // = TPICSRCK;
    int _latchPin; // = TPICRCK;
    ShiftRegisterTPIC6B595<numberOfShiftRegisters>* _tpic;//(serialDataPin, clockPin, latchPin);
};

extern LedControllerClass LedController;

