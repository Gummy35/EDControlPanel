#include "I2CDeviceDisplay.h"

char *buffer[4];
int yPos = 0;

const char *I2CDisplayBlankLine = "                    \0";

void clearBuffer()
{
  for (int i = 0; i < 4; i++)
    strncpy(buffer[i], I2CDisplayBlankLine, 21);
}

I2CDeviceDisplay::I2CDeviceDisplay(const char *name) : I2CDevice(0x27, name, 0)
{
  for (int i = 0; i < 4; i++)
    buffer[i] = (char *)malloc(21);
  clearBuffer();
}

bool I2CDeviceDisplay::Init()
{
  if (IsPresent)
  {
    Serial.printf((const char *)F("Init : %s"), Name);
    _display = new LiquidCrystal_I2C((pcf8574Address)(Addr));
    _display->begin(20, 4, LCD_5x8DOTS, Wire);
    delay(1000);
    _display->backlight();
    _display->clear();
    return true;
  }
  return false;
}

void I2CDeviceDisplay::Reset()
{
  _display->begin(20, 4, LCD_5x8DOTS, Wire);
  delay(100);
  _display->backlight();
  _display->clear();
  for (int y = 0; y < 4; y++)
  {
    _display->setCursor(0, y);
    _display->print(buffer[y]);
  }
}

void I2CDeviceDisplay::clear()
{
  clearBuffer();
  if (IsPresent)
  {
    _display->clear();
  }
}

void I2CDeviceDisplay::PrintAt(const char *str, int y)
{
  if (IsPresent)
  {
    _display->setCursor(0, y);
    _display->print(I2CDisplayBlankLine);
    bool scrollMode = strncmp(str, "$", 1) == 0;
    if (scrollMode)
    {
      strncpy(buffer[y], str + 1, 20);
      _display->blink();
      for (int i = 0; i < strlen(buffer[y]); i++)
      {
        _display->setCursor(i, y);
        _display->print(buffer[y][i]);
        delay(_scrollDelay);
      }
    }
    else
    {
      _display->noBlink();
      strncpy(buffer[y], str, 20);
      _display->setCursor(0, y);
      _display->print(buffer[y]);
    }
  }
}

void I2CDeviceDisplay::LogDisplay(const char *logString)
{
  // Serial.println(logString);
  if (IsPresent)
  {
    _display->clear();
    if (strcmp(logString, "^") == 0)
    {
      yPos = 0;
      clearBuffer();
      return;
    }
    if (yPos > 3)
    {
      for (int i = 0; i < 3; i++)
        strncpy(buffer[i], buffer[i + 1], 20);
      yPos = 3;
    }

    for (int y = 0; y < yPos; y++)
    {
      _display->setCursor(0, y);
      _display->print(buffer[y]);
    }

    PrintAt(logString, yPos++);
  }
}