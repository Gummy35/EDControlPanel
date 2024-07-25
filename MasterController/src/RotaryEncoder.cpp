#include <RotaryEncoder.h>
#include <Arduino.h>
#include <functional>

#define BUTTON_DEBOUNCE 25
#define ROTARY_DEBOUNCE 25

void IRAM_ATTR switchInterruptHandler(void * arg) {
	RotaryEncoder* object=(RotaryEncoder*)arg;
  object->switchIsrFlag = true;
}

RotaryEncoder::RotaryEncoder(int id, uint8_t clkPin, uint8_t dtPin, uint8_t swPin)
{
  Id = id;
  _swPin = swPin;
  _clkPin = clkPin;
  _dtPin = dtPin;
  onClick = nullptr;
  onLeft = nullptr;
  onRight = nullptr;
}

bool RotaryEncoder::Init()
{
    _encoder.attachHalfQuad(_dtPin, _clkPin);
    _encoder.setCount(0);
    pinMode(_swPin, INPUT_PULLUP);
    _swInterruptNum = digitalPinToInterrupt(_swPin);
    attachInterruptArg(_swInterruptNum, switchInterruptHandler, this, CHANGE);
    _previousPress = millis();
    _previousRotCheck = millis();
    return true;
}

void RotaryEncoder::Handle()
{
  if((millis() - _previousPress) > BUTTON_DEBOUNCE && switchIsrFlag)
  {
    _previousPress = millis();
    bool callHandler = false;
    if(digitalRead(_swPin) == LOW && _previousState == HIGH)
    {
      _pressCount++;
      _previousState = LOW;
      callHandler = true;    
    }
    else if(digitalRead(_swPin) == HIGH && _previousState == LOW)
    {
      _previousState = HIGH;
    }
    switchIsrFlag = false;
    if (callHandler && (onClick != nullptr))
      onClick(this, this->Id);
  }

  if ((millis() - _previousRotCheck) > ROTARY_DEBOUNCE)
  {
    _previousRotCheck = millis();
    _previousRotCount = _rotCount;
    _rotCount = GetRotCount();
    int delta = _rotCount - _previousRotCount;
    if ((delta < 0) && (onLeft != nullptr)) {
      onLeft(this, Id, -delta);
    } 
    else if ((delta > 0) && (onRight != nullptr)) {
      onRight(this, Id, delta);
    }
  }
}

int RotaryEncoder::HasPress(bool resetCounter)
{
    int res = _pressCount - _previousPressCount;
    if (resetCounter) _previousPressCount = _pressCount;
    return res;
}

int64_t RotaryEncoder::GetRotCount()
{
    return _encoder.getCount() / 2;
}

unsigned long RotaryEncoder::GetPressCount()
{
    return _pressCount;
}

void RotaryEncoder::setOnClick(std::function<void(RotaryEncoder *sender, int senderId)> handler)
{
  onClick = handler;
}

void RotaryEncoder::setOnLeft(std::function<void(RotaryEncoder *sender, int senderId, int delta)> handler)
{
  onLeft = handler;
}

void RotaryEncoder::setOnRight(std::function<void(RotaryEncoder *sender, int senderId, int delta)> handler)
{
  onRight = handler;
}
