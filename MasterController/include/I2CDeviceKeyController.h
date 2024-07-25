#pragma once

#include <I2CDevice.h>
#include "KeypadPCF8575.h"

class I2CDeviceKeyController : public I2CDevice
{
public:
    I2CDeviceKeyController(const char *name, uint8_t interruptPin, void (*listener)(uint8_t[], uint8_t[], uint8_t, uint8_t));
    virtual bool Init();
    void Handle();
    KeypadPCF8575 *Keypad;

protected:
    PCF8575 *IOExpander;

    void (*keypadEvent)(uint8_t[], uint8_t[], uint8_t, uint8_t);
};
