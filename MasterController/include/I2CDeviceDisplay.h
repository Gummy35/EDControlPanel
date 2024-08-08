#pragma once

#include <I2CDevice.h>
#include "LiquidCrystal_I2C.h"
#include <map>

struct DisplayPage
{
    const char *BlankLine = "                    \0";
    char* lines[4];

    void clearBuffer() {
        for(int i=0;i<4;i++)
            memcpy(lines[i], BlankLine, 21);
    }

    DisplayPage() {
        for(int i=0;i<4;i++)
            lines[i] = (char *)malloc(21);
        clearBuffer();
    }

    bool IsEmpty()
    {
        return (lines[0][0] == ' ') || (lines[0][0] == 0);
    }

    void Assign(DisplayPage* page) {
        for(int i=0;i<4;i++)
            memcpy(lines[i], page->lines[i], 21);
    }
};


class I2CDeviceDisplay : public I2CDevice, public Print
{
public:
    I2CDeviceDisplay(const char *name);
    virtual bool Init();
    void LogDisplay(const char *logString);
    void CreateChar(uint8_t cgramAddress, uint8_t *cgramChar);
    void PrintAt(const char *str, int y);
    void clear();
    void Reset();
    LiquidCrystal_I2C* GetLcdController();
    size_t write(uint8_t) override;
    size_t write(const uint8_t* buffer, size_t size) override;
//    void SetI2CMutex(SemaphoreHandle_t mutex);
private:
    DisplayPage buffer;
    LiquidCrystal_I2C *_display;
    const int _scrollDelay = 25;
    std::map<uint8_t, uint8_t*> _customChars;
    void _loadCustomChars();

//    SemaphoreHandle_t i2cMutex = nullptr;
};
