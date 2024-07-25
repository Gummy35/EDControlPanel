#pragma once

#include <ESP32Encoder.h>
#include <Arduino.h>

class RotaryEncoder
{
public:
    RotaryEncoder(int id, uint8_t clkPin, uint8_t dtPin, uint8_t swPin);
    bool Init();
    void Handle();
    int HasPress(bool resetCounter = true);
    int64_t GetRotCount();
    unsigned long GetPressCount();
    // void switchInterruptHandler();
    volatile bool switchIsrFlag;
    void setOnClick(std::function<void(RotaryEncoder* sender, int senderId)> handler);
    void setOnLeft(std::function<void(RotaryEncoder* sender, int senderId, int delta)> handler);
    void setOnRight(std::function<void(RotaryEncoder* sender, int senderId, int delta)> handler);

    int Id;

protected:
    ESP32Encoder _encoder;
    uint8_t _clkPin;
    uint8_t _dtPin;
    uint8_t _swPin;
    int _swInterruptNum;
    std::function<void(RotaryEncoder* sender, int senderId)> onClick;
    std::function<void(RotaryEncoder* sender, int senderId, int delta)> onLeft;
    std::function<void(RotaryEncoder* sender, int senderId, int delta)> onRight;
    unsigned long _previousPress;
    unsigned long _previousRotCheck;
    int64_t _rotCount = 0;
    int64_t _previousRotCount = 0;
    unsigned long _pressCount = 0;
    unsigned long _previousPressCount = 0;
    int _previousState = HIGH;
};
