#pragma once

#include <I2CDevice.h>
#include "LiquidCrystal_I2C.h"

class I2CDeviceDisplay : public I2CDevice
{
public:
    I2CDeviceDisplay(const char *name);
    virtual bool Init();
    void LogDisplay(const char *logString);
    void PrintAt(const char *str, int y);
    void clear();
    void Reset();

private:
    LiquidCrystal_I2C *_display;
    const int _scrollDelay = 25;
};
