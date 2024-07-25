#include <Arduino.h>
#include <Wire.h>
#include <ShiftRegisterTPIC6B595.h>
#include <EDIpcProtocolMaster.h>
#include <DeviceManager.h>
#include <I2CDevice.h>
#include <I2CDeviceComMcu.h>
#include <I2CDeviceDisplay.h>
#include <I2CDeviceKeyController.h>
#include <LedController.h>
#include <LittleFS.h>
#include <EDControllerWebServer.h>
#include <WebSerial.h>
#include <LedController.h>
#include <Logger.h>
#include <RotaryEncoder.h>
#include <ActionMapper.h>

#define SDA1 21
#define SCL1 22

#define SDA2 17
#define SCL2 16

#define COM_MCU_SIGNAL_PIN 26

#define TPICRCK 4
#define TPICSRCK 23
#define TPICSERIN 27

#define PCF8575INT 25

unsigned long last_print_time = millis();
volatile bool hasLedChanges = false;
unsigned long lastLedUpdateTime = millis();

I2CDeviceComMcu *comMcu;
I2CDeviceDisplay *display;
I2CDeviceKeyController *keyController;
EDIpcProtocolMaster *EDIpcProtocol;
DeviceManager deviceManager;
TaskHandle_t TaskKeypad, TaskComms;

RotaryEncoder *encoder1 = new RotaryEncoder(1, 13, 18, 19);
RotaryEncoder *encoder2 = new RotaryEncoder(2, 32, 33, 34);
RotaryEncoder *encoder3 = new RotaryEncoder(3, 35, 36, 39);

void keypadEvent(uint8_t pressed[], uint8_t released[], uint8_t pressCount, uint8_t releaseCount)
{
  char c;
  // Serial.printf("got event %i for %c\n", keypad->getState(), key);
  for (int i = 0; i < pressCount; i++)
  {
    c = keyController->Keypad->keymap[pressed[i]];
    WebSerial.printf("Pressed %d\n", c);
    ActionMapper.TriggerActionItem(c, true, 1);
    // if (c == '0')
    //   ledController.setNoUpdate(LED_SRV1, HIGH);
    // if (c == '8')
    //   ledController.setNoUpdate(LED_SRV2, HIGH);
    // if (c == '1')
    //   ledController.setNoUpdate(LED_SRV3, HIGH);
    // if (c == '9')
    //   ledController.setNoUpdate(LED_SRV4, HIGH);
  }

  for (int i = 0; i < releaseCount; i++)
  {
    c = keyController->Keypad->keymap[released[i]];
    WebSerial.printf("Released %d\n", c);
    ActionMapper.TriggerActionItem(c, false, 1);

    // if (c == '0')
    //   ledController.setNoUpdate(LED_SRV1, LOW);
    // if (c == '8')
    //   ledController.setNoUpdate(LED_SRV2, LOW);
    // if (c == '1')
    //   ledController.setNoUpdate(LED_SRV3, LOW);
    // if (c == '9')
    //   ledController.setNoUpdate(LED_SRV4, LOW);
  }
  // ledController.updateRegisters();
}

void TaskKeypadCode(void *pvParameters)
{
  for (;;)
  {
    keyController->Handle();
    vTaskDelay(5);
    // ledController.updateRegisters();
  }
}

void TaskCommsCode(void *pvParameters)
{
  for (;;)
  {
    comMcu->Handle();
    vTaskDelay(2);
  }
}

void onRotaryClick(RotaryEncoder *sender, int senderId)
{
  char buffer[20];
  sprintf(buffer, "Btn %d cked %d", senderId, sender->GetPressCount());
  display->PrintAt(buffer, 0);
}

void onRotaryLeft(RotaryEncoder *sender, int senderId, int delta)
{
  char buffer[20];
  sprintf(buffer, "Bt %d l %d p %d", senderId, delta, sender->GetRotCount());
  display->PrintAt(buffer, 0);
}

void onRotaryRight(RotaryEncoder *sender, int senderId, int delta)
{
  char buffer[20];
  sprintf(buffer, "Bt %d r %d p %d", senderId, delta, sender->GetRotCount());
  display->PrintAt(buffer, 0);
}

void InitDevices()
{
  byte devId = 0;

  Logger.SetLogger([](const char *logString)
                   {
    display->LogDisplay(logString);
    WebSerial.println(logString); });

  Serial.begin(115200);
  while (!Serial)
    delay(10);
  Wire.setPins(SDA1, SCL1);
  Wire.setClock(400000);
  Wire1.setPins(SDA2, SCL2);
  Wire1.setClock(400000);
  //Wire.begin(SDA1, SCL1, 400000);  // SDA pin 21, SCL pin 22 TTGO TQ
  //Wire1.begin(SDA2, SCL2, 400000); // SDA2 slave at 0x12, pin 17, SCL2 pin 16
  Wire.begin();
  Wire1.begin();

  display = new I2CDeviceDisplay((const char *)F("Display"));
  comMcu = new I2CDeviceComMcu((const char *)F("ComMcu"), COM_MCU_SIGNAL_PIN);
  keyController = new I2CDeviceKeyController((const char *)F("Keys controller"), PCF8575INT, &keypadEvent);

  deviceManager.AddDevice(display);
  deviceManager.AddDevice(comMcu);
  deviceManager.AddDevice(keyController);

  while (!comMcu->Detect(&Wire, &Wire1, -1))
  {
    Serial.println("Waiting slave MCU");
    delay(500);
  }

  deviceManager.Init();
  LedController.Init(TPICRCK, TPICSRCK, TPICSERIN);
  encoder1->Init();
  encoder2->Init();
  encoder3->Init();

  encoder1->setOnClick(onRotaryClick);
  encoder1->setOnLeft(onRotaryLeft);
  encoder1->setOnRight(onRotaryRight);
  encoder2->setOnClick(onRotaryClick);
  encoder2->setOnLeft(onRotaryLeft);
  encoder2->setOnRight(onRotaryRight);
  encoder3->setOnClick(onRotaryClick);
  encoder3->setOnLeft(onRotaryLeft);
  encoder3->setOnRight(onRotaryRight);

  EDIpcProtocol = new EDIpcProtocolMaster(comMcu);
  EDIpcProtocol->begin();

}

void SetupWebSerialCommands()
{
  WebSerial.onMessage([&](uint8_t *data, size_t len)
  {
    Serial.printf("Received %u bytes from WebSerial: ", len);
    Serial.write(data, len);
    Serial.println();
    WebSerial.println("Received Data...");
    String d = "";
    for(size_t i=0; i < len; i++){
      d += char(data[i]);
    }
    WebSerial.println(d); 
    if (d.equals("help"))
    {
      WebSerial.println("keymap.[default|save|load]");
      WebSerial.println("reboot");
    }
    else if (d.equals("keymap.default"))
    {
      ActionMapper.LoadDefaultMap();
    } 
    else if (d.equals("keymap.save"))
    {
      ActionMapper.Save();
    }
    else if (d.equals("keymap.load"))
    {
      ActionMapper.Load();
    }
    else if (d.equals("reboot"))
    {
      LittleFS.end();
      ESP.restart();
    }
  });
}

void setup()
{

  LittleFS.begin();
  EDControllerWebServer.begin();
  InitDevices();
  // Logger.Log("v1.4");
  EDControllerWebServer.showIp();
  delay(2000);

  ActionMapper.Init();

  ActionMapper.registerSendKeyHandler([](uint8_t keyCode, bool pressed, uint8_t count)
  {
    KeyEvent event;
    event.count = count;
    event.pressed = pressed;
    event.key = keyCode;
    Serial.printf("sending key %d\n", keyCode);

    EDIpcProtocol->sendKey(event);
  });

  LedController.Test();

  SetupWebSerialCommands();

  display->LogDisplay("^");
  display->LogDisplay("$All systems check");
  display->LogDisplay("$complete.");
  display->LogDisplay("");
  delay(1000);
  LedController.SetAllOn();
  delay(1000);
  //  display->LogDisplay("$1.4");
  display->LogDisplay("$Ready to engage.");
  // display->LogDisplay("$Hello Professeur");
  // display->LogDisplay("$Chandra. Ici HAL.");
  // delay(2000);
  // display->LogDisplay("$Je suis pret pour");
  // display->LogDisplay("$ma premiere lecon.");
  xTaskCreatePinnedToCore(TaskKeypadCode, "TaskKeypad", 10000, NULL, 1, &TaskKeypad, 0);
  xTaskCreatePinnedToCore(TaskCommsCode, "TaskComms", 10000, NULL, 1, &TaskComms, 1);

  delay(1000);
  EDIpcProtocol->pingSlave();
}

unsigned long resetDisplay = millis();

void loop()
{
  if ((unsigned long)(millis() - resetDisplay) > 60000)
  {
    display->Reset();
    resetDisplay = millis();
  }

  
  //  delay(1000);
  // ElegantOTA.loop();

  // if ((unsigned long)(millis() - lastLedUpdateTime) > 500)
  // {
  //   // if (loopCount % 10 == 0)
  //   // {
  //   //   LedController.SetAllOff();
  //   // }
  //   // if (loopCount % 10 == 5)
  //   // {
  //   //   LedController.SetAllOn();
  //   // }
  //   loopCount++;
  //   lastLedUpdateTime = millis();
  //   hasLedChanges = !hasLedChanges;

  //   LedController.SetGroup(LEDS_TARGETING, sizeof(LEDS_TARGETING), hasLedChanges);

  // }

  EDControllerWebServer.loop();

  encoder1->Handle();
  encoder2->Handle();
  encoder3->Handle();

  //  if((millis() - previousPress) > 50)
  //  {
  //   char buffer[20];
  //   int c1 = encoder1->HasPress(false);
  //   int c2 = encoder2->HasPress(false);
  //   int c3 = encoder3->HasPress(false);

  //   sprintf(buffer, "r1 %d r2 %d r3 %d", c1, c2, c3);
  //   display->printAt(buffer, 3);
  //   previousPress = millis();
  //  }
  //comMcu->Handle();
}
