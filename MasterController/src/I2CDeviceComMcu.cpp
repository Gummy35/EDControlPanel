#include "I2CDeviceComMcu.h"
#include <EDIpcProtocolMaster.h>
#include <Logger.h>
// #include <OTAUpdate.h>

// OTAUpdate *otaUpdate = new OTAUpdate();

volatile bool _hasComMcuInterrupt = false;

void IRAM_ATTR _comMcuInterruptHandler()
{
    _hasComMcuInterrupt = true;
}

I2CDeviceComMcu::I2CDeviceComMcu(const char *name, uint8_t interruptPin) : I2CDevice(0x12, name, interruptPin)
{
}

bool I2CDeviceComMcu::Init()
{
    if (IsPresent)
    {
        char buffer[100];
        snprintf_P(buffer, 99, (const char *)F("Init : %s, interrupt pin %d"), Name, InterruptPin);
        Logger.Log(buffer);
        pinMode(InterruptPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(InterruptPin), _comMcuInterruptHandler, FALLING);
    }
    return false;
}

void I2CDeviceComMcu::Handle()
{
    if (_hasComMcuInterrupt)
    {
        uint8_t changes = EDIpcProtocolMaster::instance->retrieveChanges();
        // if (changes & UPDATE_CATEGORY::WIFI_CREDS)
        // {
        //     // if (otaUpdate->_active)
        //     //     otaUpdate->stop();
        //     // if (strlen(EDIpcProtocolMaster::instance->wifiCredentials.ssid) > 0)
        //     //     otaUpdate->begin("edcontrolpanel", EDIpcProtocolMaster::instance->wifiCredentials.ssid, EDIpcProtocolMaster::instance->wifiCredentials.password);
        // }
        _hasComMcuInterrupt = false;
    }
    EDIpcProtocolMaster::instance->sendChanges();
}