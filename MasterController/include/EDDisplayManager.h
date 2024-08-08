#pragma once

#include <I2CDeviceDisplay.h>
#include <EDGameVariables.h>
#include <vector>

#define CAROUSEL_PAGE_DURATION 5000
#define BLINK_INTERVAL 500

class EDDisplayManager
{
public:
    DisplayPage* StatusPage;
    DisplayPage* LocationPage;
    DisplayPage* CommanderPage;
    DisplayPage* NavPage;
    DisplayPage* NavRoutePage;
    DisplayPage* ErrorPage;

    EDDisplayManager(I2CDeviceDisplay* display);
    void Handle();
    void Display(DisplayPage* page);
    void BlinkLines(bool showBlinkLines);
    void Lock();
    void Unlock();
    void UpdatePages();
    void Invalidate();
protected:
    I2CDeviceDisplay* _display;
    unsigned long lastTick = 0;
    unsigned long lastBlink = 0;
    int _pageIdx = 0;
    std::vector<DisplayPage*> _Carousel;
    DisplayPage* _current;
    DisplayPage _buffer;
    SemaphoreHandle_t _lock;
};