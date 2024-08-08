#include <LedController.h>
#include <Logger.h>
#include <EDGameVariables.h>

void LedControllerClass::Init(int rck, int srck, int serin)
{
  memset(_ledStatus, 0, numberOfShiftRegisters * 8);
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

void LedControllerClass::SetGroup(const uint8_t *Ledgroup, int size, uint8_t value, bool updateRegisters)
{
  for (int i = 0; i < size; i++)
  {
    // WebSerial.printf("Led %d : %d", i, Ledgroup[i]);
    _tpic->setNoUpdate(Ledgroup[i], value);
  }
  if (updateRegisters)
    _tpic->updateRegisters();
}

void LedControllerClass::SetGroupStatus(const uint8_t *Ledgroup, int size, uint8_t value)
{
  for (int i = 0; i < size; i++)
    _ledStatus[Ledgroup[i]] = value;
}


void LedControllerClass::Update()
{
  static unsigned long lastUpdate = millis();
  uint8_t ledStatus;

  // clear all leds
  memset(_ledStatus, 0, numberOfShiftRegisters * 8);

  if (EDGameVariables.IsBeingInterdicted())
  {
    memset(_ledStatus, 2, numberOfShiftRegisters * 8);
  }
  else
  {
    SetGroupStatus(LEDS_SRV, sizeof(LEDS_SRV), EDGameVariables.IsInSRV());
    SetGroupStatus(LEDS_WING, sizeof(LEDS_WING), EDGameVariables.IsInWing());

    SetGroupStatus(LEDS_STARNAV, sizeof(LEDS_STARNAV), 1);
    SetGroupStatus(LEDS_ENERGY, sizeof(LEDS_ENERGY), 1);

    bool isFlying = (!EDGameVariables.IsDocked()) && (!EDGameVariables.IsLanded());
    bool canFsd = isFlying && (!EDGameVariables.IsFsdMassLocked()) && (!EDGameVariables.IsFsdCharging()) && (!EDGameVariables.IsFsdCooldown()) && (!EDGameVariables.IsFsdJump());

    _ledStatus[STARNAV_NEXT_SYSTEM] = (EDGameVariables.Navroute1 != nullptr) && (EDGameVariables.Navroute1[0] != ' ') && (EDGameVariables.Navroute1[0] != 0);

    SetGroupStatus(LEDS_TARGETING, sizeof(LEDS_TARGETING), isFlying && (!EDGameVariables.IsFsdJump()));

    _ledStatus[TARGETING_NEXT_GROUP] = EDGameVariables.IsHardpointsDeployed();

    if (EDGameVariables.IsFsdCharging())
    {
      _ledStatus[ENGINE_CRUISE] = 2;
      _ledStatus[ENGINE_JUMP] = 2;
    }
    else if (EDGameVariables.IsSupercruise())
    {
        _ledStatus[ENGINE_CRUISE] = 0;
        _ledStatus[ENGINE_JUMP] = 1;
    }
    else if (canFsd) {
      _ledStatus[ENGINE_CRUISE] = 1;
      _ledStatus[ENGINE_JUMP] = 1;
    }

    if (EDGameVariables.IsInDanger())
    {
      SetGroupStatus(LEDS_DEFENSE, sizeof(LEDS_DEFENSE), 2);
    }
    else
    {
      if (isFlying)
      {
        // Should check equipment status
        SetGroupStatus(LEDS_DEFENSE, sizeof(LEDS_DEFENSE), 1);
      }
    }

    _ledStatus[ENGINE_DISENGAGE] = EDGameVariables.IsFsdJump() || EDGameVariables.IsSupercruise();
    _ledStatus[ENGINE_FULL_STOP] = isFlying;
    // SetGroupStatus(LEDS_FIGHTERS, sizeof(LEDS_FIGHTERS), EDGameVariables.IsInMainShip() &&  );


    if (EDGameVariables.IsHudAnalysisMode())
      SetGroupStatus(LEDS_SCANNER, sizeof(LEDS_SCANNER), 1);
  }

  if (millis() - lastUpdate > 500)
  {
    bool alternate = ((millis() - lastUpdate) % 1000) < 500;
    for(int i=0;i<numberOfShiftRegisters*8;i++)
    {
      ledStatus = _ledStatus[i];
      if (ledStatus == 2) ledStatus = alternate;
      _tpic->setNoUpdate(i, ledStatus);
    }
    _tpic->updateRegisters();
  }
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
  _tpic->setAllLow();
  delay(1000);
}

LedControllerClass LedController;