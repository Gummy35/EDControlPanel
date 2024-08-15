#include "I2CDeviceDisplay.h"

// #define PROTECT_I2C_ACCESS(MyCode) \
//     if (i2cMutex != nullptr) { \
//         if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) { \
//             MyCode; \
//             xSemaphoreGive(i2cMutex); \
//         } \
//     }

#define PROTECT_I2C_ACCESS(MyCode) MyCode;

const char *BlankLine = "                    \0";
int yPos = 0;

I2CDeviceDisplay::I2CDeviceDisplay(const char *name) : I2CDevice(0x27, name, 0)
{
  buffer.clearBuffer();
}

bool I2CDeviceDisplay::Init()
{
  if (IsPresent)
  {
    Serial.printf((const char *)F("Init : %s"), Name);
    _display = new LiquidCrystal_I2C((pcf8574Address)(Addr));
    _display->begin(20, 4, LCD_5x8DOTS, Wire);
    delay(1000);
    _loadCustomChars();
    _display->backlight();
    _display->clear();
    return true;
  }
  return false;
}

void I2CDeviceDisplay::Reset()
{
  PROTECT_I2C_ACCESS(_display->begin(20, 4, LCD_5x8DOTS, Wire));
  delay(100);
  PROTECT_I2C_ACCESS(_display->backlight();_display->clear())
  _loadCustomChars();
  for (int y = 0; y < 4; y++)
  {
    PROTECT_I2C_ACCESS(_display->setCursor(0, y);_display->print(buffer.lines[y]))
  }
}

LiquidCrystal_I2C *I2CDeviceDisplay::GetLcdController()
{
    return _display;
}

size_t I2CDeviceDisplay::write(uint8_t c)
{
    write(&c, 1);
    return 1;
}

size_t I2CDeviceDisplay::write(const uint8_t *buffer, size_t size)
{
  LogDisplay((char *)buffer);
  return size;
}

// void I2CDeviceDisplay::SetI2CMutex(SemaphoreHandle_t mutex)
// {
//   i2cMutex = mutex;
// }

void I2CDeviceDisplay::clear()
{
  buffer.clearBuffer();
  if (IsPresent)
  {
    PROTECT_I2C_ACCESS(_display->clear())
  }
}

void I2CDeviceDisplay::PrintAt(const char *str, int y)
{
  if (IsPresent)
  {
    if (str[0] == '~') str++;
    PROTECT_I2C_ACCESS(_display->setCursor(0, y); _display->print(BlankLine))

    bool scrollMode = strncmp(str, "$", 1) == 0;
    if (scrollMode)
    {
      strncpy(buffer.lines[y], str + 1, 20);
      PROTECT_I2C_ACCESS(_display->blink())
      for (int i = 0; i < strlen(buffer.lines[y]); i++)
      {
        PROTECT_I2C_ACCESS(_display->setCursor(i, y);_display->print(buffer.lines[y][i]))
        delay(_scrollDelay);
      }
    }
    else
    {
      PROTECT_I2C_ACCESS(_display->noBlink())
      strncpy(buffer.lines[y], str, 20);
      PROTECT_I2C_ACCESS(_display->setCursor(0, y); _display->print(buffer.lines[y]))
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
      buffer.clearBuffer();
      return;
    }
    if (yPos > 3)
    {
      for (int i = 0; i < 3; i++)
        strncpy(buffer.lines[i], buffer.lines[i + 1], 20);
      yPos = 3;
    }

    for (int y = 0; y < yPos; y++)
    {
      PROTECT_I2C_ACCESS(_display->setCursor(0, y); _display->print(buffer.lines[y]))
    }

    PrintAt(logString, yPos++);
  }
}

void I2CDeviceDisplay::_loadCustomChars()
{
  for (const auto& kv : _customChars) {
    _display->createChar(kv.first, kv.second);
  }
}

void I2CDeviceDisplay::CreateChar(uint8_t cgramAddress, uint8_t *cgramChar)
{
  _customChars[cgramAddress] = cgramChar;
}
