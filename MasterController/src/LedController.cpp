#include <LedController.h>
#include <Logger.h>

void LedControllerClass::Init(int rck, int srck, int serin)
{
    _serialDataPin = serin;
    _clockPin = srck;
    _latchPin = rck;

    _tpic = new ShiftRegisterTPIC6B595<numberOfShiftRegisters>(_serialDataPin, _clockPin, _latchPin);
}

void LedControllerClass::SetAllOff()
{
    _tpic->setAllLow();
}

void LedControllerClass::SetAllOn()
{
    _tpic->setAllHigh();
}

void LedControllerClass::SetGroup(const uint8_t *Ledgroup, int size, uint8_t value)
{
  for (int i = 0; i < size; i++)
  {
    // WebSerial.printf("Led %d : %d", i, Ledgroup[i]);
    _tpic->setNoUpdate(Ledgroup[i], value);
  }
  _tpic->updateRegisters();
}

void LedControllerClass::Test()
{
  Serial.println("Testing Led controller");
  _tpic->setAllLow();
  for (int i = 0; i < 48; i++)
  {
    _tpic->set(i, HIGH);
    delay(50);
    _tpic->set(i, LOW);
  }

  SetGroup(LEDS_SRV, sizeof(LEDS_SRV), HIGH);
  Logger.Log("Testing SRV");
  delay(300);
  SetGroup(LEDS_SRV, sizeof(LEDS_SRV), LOW);
  SetGroup(LEDS_FIGHTERS, sizeof(LEDS_FIGHTERS), HIGH);
  Logger.Log("Testing Fighters");
  delay(300);
  SetGroup(LEDS_FIGHTERS, sizeof(LEDS_FIGHTERS), LOW);
  SetGroup(LEDS_WING, sizeof(LEDS_WING), HIGH);
  Logger.Log("Testing Wing");
  delay(300);
  SetGroup(LEDS_WING, sizeof(LEDS_WING), LOW);
  SetGroup(LEDS_STARNAV, sizeof(LEDS_STARNAV), HIGH);
  Logger.Log("Testing Starnav");
  delay(300);
  SetGroup(LEDS_STARNAV, sizeof(LEDS_STARNAV), LOW);
  SetGroup(LEDS_DEFENSE, sizeof(LEDS_DEFENSE), HIGH);
  Logger.Log("Testing Defense");
  delay(300);
  SetGroup(LEDS_DEFENSE, sizeof(LEDS_DEFENSE), LOW);
  SetGroup(LEDS_ENERGY, sizeof(LEDS_ENERGY), HIGH);
  Logger.Log("Testing Energy");
  delay(300);
  SetGroup(LEDS_ENERGY, sizeof(LEDS_ENERGY), LOW);
  SetGroup(LEDS_SCANNER, sizeof(LEDS_SCANNER), HIGH);
  Logger.Log("Testing Scanner");
  delay(300);
  SetGroup(LEDS_SCANNER, sizeof(LEDS_SCANNER), LOW);
  SetGroup(LEDS_TARGETING, sizeof(LEDS_TARGETING), HIGH);
  Logger.Log("Testing Targeting");
  delay(300);
  SetGroup(LEDS_TARGETING, sizeof(LEDS_TARGETING), LOW);
  SetGroup(LEDS_ENGINE, sizeof(LEDS_ENGINE), HIGH);
  Logger.Log("Testing Engine");
  delay(300);
  SetGroup(LEDS_ENGINE, sizeof(LEDS_ENGINE), LOW);
}

LedControllerClass LedController;