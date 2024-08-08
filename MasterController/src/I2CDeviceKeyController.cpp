#include "I2CDeviceKeyController.h"
#include <Logger.h>
#include <PanelActions.h>

byte KeypadRowPins[8] = {0, 1, 2, 3, 4, 5, 6, 7};       // connect to the row pinouts of the keypad
byte KeypadColPins[8] = {8, 9, 10, 11, 12, 13, 14, 15}; // connect to the column pinouts of the keypad

const byte KeypadRows = 8; // four rows
const byte KeypadCols = 8; // four columns
const uint KeypadDebouncems = 50;
const uint KeypadHoldms = 50;
const uint8_t KeypadAddress = 0x27;
const uint8_t IOExpanderInitValue = 0xFF;

volatile bool _hasKeypadInterrupt = false;

void IRAM_ATTR _keypadInterruptHandler()
{
    _hasKeypadInterrupt = true;
}

char KeypadKeys[KeypadRows][KeypadCols] =
    {
        {SRV_1,             SRV_2,              SHIP_SILENT_RUNNING,    'x',                    STARNAV_GALAXY_MAP,     'x',                        DEFENSE_SHIELD_CELL,    ENERGY_SYSTEMS},
        {SRV_3,             SRV_4,              SHIP_JETTISON_CARGO,    'x',                    STARNAV_SYSTEM_MAP,     'x',                        DEFENSE_HEAT_SINK,      ENERGY_ENGINE},
        {FIGHTER_DEFEND,    FIGHTER_ATTACK,     SHIP_EMERGENCY_STOP,    'x',                    'x',                    'x',                        DEFENSE_CHAFF,          ENERGY_WEAPONS},
        {FIGHTER_HOLDPOS,   FIGHTER_HOLDFIRE,   SHIP_CARGO_SCOOP,       'x',                    STARNAV_NEXT_SYSTEM,    'x',                        DEFENSE_ECM,            ENERGY_BALANCE},
        {FIGHTER_FOLLOW,    FIGHTER_DOCK,       SHIP_LIGHTS,            COCKPIT_MODE,           'x',                    TARGETING_MOST_HOSTILE,     'x',                    ENGINE_CRUISE},
        {WING_1,            FIGHTER_FIREATWILL, SHIP_HARDPOINTS,        SCANNER_DSD,            SCANNER_ACS,            TARGETING_MAIN_THREAT,      TARGETING_NEXT_TARGET,  ENGINE_JUMP},
        {WING_3,            WING_NAVLOCK,       SHIP_LANDING_GEARS,     SCANNER_PREVIOUS_FILTER,'x',                    TARGETING_PREVIOUS_THREAT,  TARGETING_NEXT_SUBSYS,  ENGINE_DISENGAGE},
        {WING_2,            WING_TARGET,        SHIP_FLIGHT_ASSIST,     SCANNER_NEXT_FILTER,    'x',                    TARGETING_NEXT_THREAT,      TARGETING_NEXT_GROUP,   ENGINE_FULL_STOP},
};

I2CDeviceKeyController::I2CDeviceKeyController(
    const char *name, uint8_t interruptPin,
    void (*listener)(uint8_t[], uint8_t[], uint8_t, uint8_t)) : I2CDevice(0x20, 0x23, name, interruptPin)
{
    keypadEvent = listener;
}

bool I2CDeviceKeyController::Init()
{
    if (IsPresent)
    {
        char buffer[100];
        snprintf_P(buffer, 99, (const char *)F("Init : %s, interrupt pin %d"), Name, InterruptPin);
        Logger.Log(buffer);
        IOExpander = new PCF8575(Addr, Wire);
        // keypad = new FixedKeypadPCF8575(IOExpander, makeKeymap(KeypadKeys));
        Keypad = new KeypadPCF8575(IOExpander, makeKeymap(KeypadKeys), KeypadRowPins, KeypadColPins, 8, 8);
        Keypad->addChangesEventListener(keypadEvent);
        while (IOExpander->begin() == false)
        {
            Serial.println("ERROR: cannot communicate to keypad.\n");
            delayMicroseconds(100000);
        }
        Keypad->setDebounceTime(KeypadDebouncems);
        Keypad->setHoldTime(KeypadHoldms);
        // _hasKeypadInterrupt = false;
        // pinMode(InterruptPin, INPUT);
        // attachInterrupt(InterruptPin, _keypadInterruptHandler, FALLING);
        // Keypad->enableInterrupt();
        return true;
    }
    return false;
}

void I2CDeviceKeyController::Handle()
{
    // if (_hasKeypadInterrupt)
    // {
    //   if (Keypad->getKeys()) {

    //     I2CDevice::Log("keypad interrupt");
    //     _hasKeypadInterrupt = false;
    //     Keypad->enableInterrupt();
    //   }
    // }
    Keypad->getKeys();
}

bool I2CDeviceKeyController::IsPressed(uint8_t keyCode)
{
    return Keypad->isPressed(keyCode);
}

// void I2CDeviceKeyController::SetI2CMutex(SemaphoreHandle_t mutex)
// {
//     Keypad->i2cMutex = mutex;
// }
