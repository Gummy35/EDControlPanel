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
#include <EDGameVariables.h>
#include <EDDisplayManager.h>

#define DEBUG

#ifdef DEBUG
#define debug(MyCode) MyCode;
#else
#endif

#define SDA1 21
#define SCL1 22

#define SDA2 17
#define SCL2 16

#define COM_MCU_SIGNAL_PIN 26

#define TPICRCK 4
#define TPICSRCK 23
#define TPICSERIN 27

#define PCF8575INT 25

// Arduino pro micro I2C slave mcu, used for communication with PC
I2CDeviceComMcu *comMcu;
// I2C LCD 20x4 display device
I2CDeviceDisplay *display;
// I2C PCF8575 Keypad controller
I2CDeviceKeyController *keyController;

// Inter MCU protocol manager
EDIpcProtocolMaster *EDIpcProtocol;

// I2C device manager; search and initialize I2C peripherals
DeviceManager deviceManager;

// FreeRTOS tasks
TaskHandle_t TaskKeypad, TaskComms, TaskLedController, TaskDisplayController;
// // I2C semaphore; I2C bus is shared amongst multiple tasks, so protect it's access
// SemaphoreHandle_t i2cMutex;

// LCD Display logic (carousel, alerts...)
EDDisplayManager *displayManager;

// 3 rotary encoders, not managed by the DeviceKeyController
RotaryEncoder *encoder1 = new RotaryEncoder(1, 13, 18, 19);
RotaryEncoder *encoder2 = new RotaryEncoder(2, 32, 33, 34);
RotaryEncoder *encoder3 = new RotaryEncoder(3, 35, 36, 39);

// Action mapper is responsible for mapping keystrikes to events to send to slave MCU.
// It must also manage conflicts between physical state and game expected state.
// For example, landing gears are managed by a keystroke in the game, but we use a on/off toggle switch
void SetupActionMapper()
{
  // Initialise action mapper
  ActionMapper.Init();

  // this handler is called by action mapper when a key has been processed and must be send somewhere.
  // here, to the com MCU
  ActionMapper.registerSendKeyHandler([](uint8_t keyCode, bool pressed, uint8_t count)
                                      {
                                        KeyEvent event;
                                        event.count = count;
                                        event.pressed = pressed;
                                        event.key = keyCode;
                                        debug(Serial.printf("sending key %d\n", keyCode))
                                            EDIpcProtocol->sendKey(event); });

  // this handler is called by action mapper when it needs to know the physical state of a toggle switch
  ActionMapper.registerGetGameStatusHandler([](uint8_t actionId) -> ActionnerState
                                            {
                                              if (actionId == SHIP_SILENT_RUNNING)
                                                return EDGameVariables.IsSilentRunning() ? ActionnerState::Active : ActionnerState::Inactive;
                                              if (actionId == SHIP_CARGO_SCOOP)
                                                return EDGameVariables.IsCargoScoopDeployed() ? ActionnerState::Active : ActionnerState::Inactive;
                                              if (actionId == SHIP_LIGHTS)
                                                return EDGameVariables.IsLightsOn() ? ActionnerState::Active : ActionnerState::Inactive;
                                              if (actionId == SHIP_HARDPOINTS)
                                                return EDGameVariables.IsHardpointsDeployed() ? ActionnerState::Active : ActionnerState::Inactive;
                                              if (actionId == SHIP_LANDING_GEARS)
                                                return EDGameVariables.IsLandingGearDown() ? ActionnerState::Active : ActionnerState::Inactive;
                                              // Flight assist is enabled by default in the game, actionning it means disabling it in the game
                                              if (actionId == SHIP_FLIGHT_ASSIST)
                                                return EDGameVariables.IsFlightAssistOff() ? ActionnerState::Inactive : ActionnerState::Active;
                                              // TODO : find how we can activate this baby
                                              if (actionId == SCANNER_DSD)
                                                return ActionnerState::Inactive;
                                              //   return EDGameVariables.IsDsdActive() ? ActionnerState::Active : ActionnerState::Inactive;
                                              // TODO : find how we can activate this baby
                                              if (actionId == SCANNER_ACS)
                                                return ActionnerState::Inactive;
                                              //   return EDGameVariables.IsAcsActive() ? ActionnerState::Active : ActionnerState::Inactive;
                                              return ActionnerState::Init; });
}

/// @brief this method is used to update action mapper toggle buttons state, without waiting for an action on it
void updateKeysLocalStatus()
{
  // debug(Serial.println("Updating toggles physical state"))
  ActionMapper.UpdateLocalStatus(SHIP_SILENT_RUNNING, keyController->IsPressed(SHIP_SILENT_RUNNING_KEYCODE));
  ActionMapper.UpdateLocalStatus(SHIP_CARGO_SCOOP, keyController->IsPressed(SHIP_CARGO_SCOOP_KEYCODE));
  ActionMapper.UpdateLocalStatus(SHIP_LIGHTS, keyController->IsPressed(SHIP_LIGHTS_KEYCODE));
  ActionMapper.UpdateLocalStatus(SHIP_HARDPOINTS, keyController->IsPressed(SHIP_HARDPOINTS_KEYCODE));
  ActionMapper.UpdateLocalStatus(SHIP_LANDING_GEARS, keyController->IsPressed(SHIP_LANDING_GEARS_KEYCODE));
  ActionMapper.UpdateLocalStatus(SHIP_FLIGHT_ASSIST, keyController->IsPressed(SHIP_FLIGHT_ASSIST_KEYCODE));
  ActionMapper.UpdateLocalStatus(SCANNER_DSD, keyController->IsPressed(SCANNER_DSD_KEYCODE));
  ActionMapper.UpdateLocalStatus(SCANNER_ACS, keyController->IsPressed(SCANNER_ACS_KEYCODE));
}

/// @brief Called by the KeyController when on or many keys have been pressed or released
void keypadEvent(uint8_t pressed[], uint8_t released[], uint8_t pressCount, uint8_t releaseCount)
{
  char c;
  for (int i = 0; i < pressCount; i++)
  {
    c = keyController->Keypad->keymap[pressed[i]];
    debug(WebSerial.printf("Pressed %d\n", c))
        ActionMapper.TriggerActionItem(c, true, 1);
  }

  for (int i = 0; i < releaseCount; i++)
  {
    c = keyController->Keypad->keymap[released[i]];
    debug(WebSerial.printf("Released %d\n", c))
        ActionMapper.TriggerActionItem(c, false, 1);
  }
}

#pragma region FreeRTOS tasks

/// @brief FreeRTOS Task, call the display manager every 100ms (update LCD infos)
void TaskDisplayCode(void *pvParameters)
{
  for (;;)
  {
    displayManager->Handle();
    vTaskDelay(100);
  }
}

/// @brief FreeRTOS Task, handle key controller, update toggle switches
void TaskKeypadCode(void *pvParameters)
{
  static unsigned long inconsistentCheckTime = millis();
  String s = "";
  String s2 = "";
  String prev_s = "";
  String prev_s2 = "";
  for (;;)
  {
    // handle keypad keystrokes
    keyController->Handle();
    vTaskDelay(5);

    // every 2000ms, check for inconsistencies
    if (millis() - inconsistentCheckTime > 2000)
    {
      // update toggle buttons physical state
      updateKeysLocalStatus();
      // retrieve all inconsistencies (ie. when a toggle is ON but should be OFF)
      std::vector<uint8_t> inconsistencies = ActionMapper.GetInconsistencies();

      prev_s = s;
      prev_s2 = s2;
      s = "";
      s2 = "";
      // prepare error labels
      for (const auto &id : inconsistencies)
      {
        if (id == SHIP_SILENT_RUNNING)
          s += "SIL ";
        if (id == SHIP_CARGO_SCOOP)
          s += "CAR ";
        if (id == SHIP_LIGHTS)
          s += "LIG ";
        if (id == SHIP_HARDPOINTS)
          s += "HAR";
        if (id == SHIP_LANDING_GEARS)
          s2 += "LAN ";
        if (id == SHIP_FLIGHT_ASSIST)
          s2 += "FLI ";
        if (id == SCANNER_DSD)
          s2 += "DSD ";
        if (id == SCANNER_ACS)
          s2 += "ACS";
      }

      // get a lock on display manager
      displayManager->Lock();

      // if error message has change since the last loop,
      if ((prev_s != s) || (prev_s2 != s2))
      {
        // clear the error page (the highest priority page)
        displayManager->ErrorPage->clearBuffer();
        // if there are errors
        if ((s.length() > 0) || (s2.length() > 0))
        {
          // build the page
          strcpy(displayManager->ErrorPage->lines[0], "~*** CHECK CMDS ***");
          strcpy(displayManager->ErrorPage->lines[1], s.c_str());
          strcpy(displayManager->ErrorPage->lines[2], s2.c_str());

          debug(Serial.println(displayManager->ErrorPage->lines[0]))
              debug(Serial.println(displayManager->ErrorPage->lines[1]))
                  debug(Serial.println(displayManager->ErrorPage->lines[2]))
        }
        // force refresh
        displayManager->Invalidate();
      }
      // release the lock
      displayManager->Unlock();
      // update timestamp
      inconsistentCheckTime = millis();
    }
  }
}

/// @brief FreeRTOS Task, handle leds
void TaskUpdateLedsCode(void *pvParameters)
{
  for (;;)
  {
    // update leds statuses every 100ms
    LedController.Update();
    vTaskDelay(100);
  }
}

/// @brief FreeRTOS Task, handle comms with slave MCU
void TaskCommsCode(void *pvParameters)
{
  uint8_t changes;
  for (;;)
  {
    // sync changes with slave MCU
    changes = comMcu->Handle();
    if (changes != 0)
    {
      // if we got data from MCU, update pages within display manager
      displayManager->UpdatePages();
    }

    // if status flags changed, update toggles remote status
    if (changes & (uint8_t)UPDATE_CATEGORY::UC_STATUS)
    {
      debug(Serial.println("New remote status received, updating toggles"))
      ActionMapper.UpdateRemoteStatus();
      displayManager->Lock();
      if (EDGameVariables.IsFsdJump()) {
        displayManager->NavPage->Assign(displayManager->NavRoutePage);
      } else {
        displayManager->NavPage->clearBuffer();
      }
      displayManager->Invalidate();
      displayManager->Unlock();
    }

    // if (changes & (uint8_t)UPDATE_CATEGORY::UC_LOCATION)
    // {
    //   displayLocation();
    // }

    vTaskDelay(5);
  }
}

#pragma endregion

#pragma region rotary buttons events

void onRotaryClick(RotaryEncoder *sender, int senderId)
{
  if (senderId == 1)
    ActionMapper.TriggerActionItem(SCANNER_FREQ_TARGET, true, 1);
  else if (senderId == 2)
    ActionMapper.TriggerActionItem(SCANNER_ZOOM_CLICK, true, 1);
  // else if (senderId == 3)
  //   ActionMapper.TriggerActionItem(TARGETING_SENSORS_CLICK, true, 1);
}

void onRotaryLeft(RotaryEncoder *sender, int senderId, int delta)
{
  if (senderId == 1)
    ActionMapper.TriggerActionItem(SCANNER_FREQ_LEFT, true, delta);
  else if (senderId == 2)
    ActionMapper.TriggerActionItem(SCANNER_ZOOM_LEFT, true, delta);
  else if (senderId == 3)
    ActionMapper.TriggerActionItem(TARGETING_SENSORS_LEFT, true, delta);
}

void onRotaryRight(RotaryEncoder *sender, int senderId, int delta)
{
  if (senderId == 1)
    ActionMapper.TriggerActionItem(SCANNER_FREQ_RIGHT, true, delta);
  else if (senderId == 2)
    ActionMapper.TriggerActionItem(SCANNER_ZOOM_RIGHT, true, delta);
  else if (senderId == 3)
    ActionMapper.TriggerActionItem(TARGETING_SENSORS_RIGHT, true, delta);
}

#pragma endregion

/// @brief Welcome message, default status page
void Welcome()
{
  displayManager->Lock();
  displayManager->StatusPage->clearBuffer();
  strcpy(displayManager->StatusPage->lines[0], "$All systems check");
  strcpy(displayManager->StatusPage->lines[1], "$complete");
  strcpy(displayManager->StatusPage->lines[3], "$Ready to engage.");
  displayManager->Invalidate();
  displayManager->Unlock();
}

void displayChars(int page)
{
  displayManager->Lock();
  displayManager->ErrorPage->clearBuffer();
  for (int i=0;i<20;i++) {
    displayManager->ErrorPage->lines[0][i] = ((page*61) + i+1) % 256;
    displayManager->ErrorPage->lines[1][i] = ((page*61) + i+21) % 256;
    displayManager->ErrorPage->lines[2][i] = ((page*61) + i+41) % 256;
    displayManager->ErrorPage->lines[3][i] = ((page*61) + i+61) % 256;
  }    
  displayManager->Invalidate();
  displayManager->Unlock();
}

/// @brief Init
void InitDevices()
{
  byte devId = 0;

  // set default logger callback
  Logger.SetLogger([](const char *logString)
                   {
                     display->LogDisplay(logString);
                     WebSerial.println(logString); });

  // Wait for serial
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  // Init main I2C
  Wire.setPins(SDA1, SCL1);
  Wire.setClock(400000);
  // Init Com MCU I2C
  Wire1.setPins(SDA2, SCL2);
  Wire1.setClock(400000);
  delay(500);
  Wire.begin();
  Wire1.begin();

  // wait 1s to avoid a race condition on Wire
  delay(500);
  // i2cMutex = xSemaphoreCreateMutex();
  // if (i2cMutex == nullptr)
  // {
  //   Serial.println("Critical : I2C mutex creation failed");
  //   while (true)
  //     ;
  // }

  // Init display device
  display = new I2CDeviceDisplay((const char *)F("Display"));
  // Init Com MCU device
  comMcu = new I2CDeviceComMcu((const char *)F("ComMcu"), COM_MCU_SIGNAL_PIN);
  // Init key controller device
  keyController = new I2CDeviceKeyController((const char *)F("Keys controller"), PCF8575INT, &keypadEvent);

  // // assign I2CMutex as display and keycontroller share the same I2C bus
  // display->SetI2CMutex(i2cMutex);
  // keyController->SetI2CMutex(i2cMutex);

  // Create the display Manager
  displayManager = new EDDisplayManager(display);

  // Add devices to device manager
  deviceManager.AddDevice(display);
  deviceManager.AddDevice(comMcu);
  deviceManager.AddDevice(keyController);

  // wait for slave Mcu
  while (!comMcu->Detect(&Wire, &Wire1, -1))
  {
    Serial.println("Waiting slave MCU");
    delay(500);
  }

  // initialize devices
  deviceManager.Init();
  // initialize led controller
  LedController.Init(TPICRCK, TPICSRCK, TPICSERIN);
  // initialize rotary encoders
  encoder1->Init();
  encoder2->Init();
  encoder3->Init();

  // assign rotary encoders events
  encoder1->setOnClick(onRotaryClick);
  encoder1->setOnLeft(onRotaryLeft);
  encoder1->setOnRight(onRotaryRight);
  encoder2->setOnClick(onRotaryClick);
  encoder2->setOnLeft(onRotaryLeft);
  encoder2->setOnRight(onRotaryRight);
  encoder3->setOnClick(onRotaryClick);
  encoder3->setOnLeft(onRotaryLeft);
  encoder3->setOnRight(onRotaryRight);

  // initialize IPC protocol
  EDIpcProtocol = new EDIpcProtocolMaster(comMcu);
  EDIpcProtocol->begin();
}

/// @brief setup Web serial commands handling
void SetupWebSerialCommands()
{
  WebSerial.onMessage([&](uint8_t *data, size_t len)
                      {
                        debug(Serial.printf("Received %u bytes from WebSerial: ", len))
                        debug(Serial.write(data, len))
                        debug(Serial.println())
                        WebSerial.println("Received Data...");
                        String d = "";
                        for (size_t i = 0; i < len; i++)
                        {
                          d += char(data[i]);
                        }
                        WebSerial.println(d);
                        if (d.equals("help"))
                        {
                          WebSerial.println("keymap.[default|save|load]");
                          WebSerial.println("slave.[ping|reset|getupdates|getalldata]");
                          //      WebSerial.println("display.[location|commander|status]");
                          WebSerial.println("test.[leds|i2clarge]");
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
                        else if (d.equals("slave.ping"))
                        {
                          EDIpcProtocol->pingSlave();
                        }
                        else if (d.equals("slave.reset"))
                        {
                          EDIpcProtocol->resetSlave();
                        }
                        else if (d.equals("slave.getupdates"))
                        {
                          EDIpcProtocol->retrieveChanges();
                        }
                        else if (d.equals("slave.getalldata"))
                        {
                          EDIpcProtocol->retrieveChanges(true);
                          displayManager->UpdatePages(); 
                        }
                        // else if (d.equals("display.location"))
                        // {
                        //   displayLocation();
                        // }
                        // else if (d.equals("display.commander"))
                        // {
                        //   displayCommander();
                        // }
                        // else if (d.equals("display.status"))
                        // {
                        //   displayStatus();
                        // }
                        else if (d.equals("test.leds"))
                        {
                          LedController.Test();
                        }
                        else if (d.equals("test.i2clarge"))
                        {
                          uint8_t buf[128];
                          comMcu->getData((uint8_t)COM_REQUEST_TYPE::CRT_MOCK, buf, 128);
                          Serial.printf("Got %s\n", buf);
                        }
                        else if (d.equals("test.display"))
                        {
                          displayChars(0);
                        }
                        else if (d.equals("test.display2"))
                        {
                          displayChars(1);
                        }
                        else if (d.equals("test.display3"))
                        {
                          displayChars(2);
                        }
                        else if (d.equals("test.display4"))
                        {
                          displayChars(3);
                        }
                      });
}


void setup()
{
  // start filesystem
  LittleFS.begin();
  // web server (ota + serial)
  EDControllerWebServer.begin();
  delay(500);
  // init devices
  InitDevices();
  // Logger.Log("v1.4");
  EDControllerWebServer.showIp();
  delay(500);

  // configure action mapper
  SetupActionMapper();
  // configure web serial commands
  SetupWebSerialCommands();

  // display welcome banner
  Welcome();

  // force a toggle buttons status update
  updateKeysLocalStatus();

  // create FreeRTOS tasks
  xTaskCreatePinnedToCore(TaskKeypadCode, "TaskKeypad", 10000, NULL, 1, &TaskKeypad, 0);
  xTaskCreatePinnedToCore(TaskDisplayCode, "TaskDisplay", 10000, NULL, 1, &TaskDisplayController, 0);
  xTaskCreatePinnedToCore(TaskCommsCode, "TaskComms", 10000, NULL, 1, &TaskComms, 1);
  xTaskCreatePinnedToCore(TaskUpdateLedsCode, "TaskLedcontroller", 10000, NULL, 1, &TaskLedController, 1);
  // wait for everyone to be ready
  delay(1000);
  // ping slave MCU
  EDIpcProtocol->pingSlave();
}

void loop()
{
  // handle web server (OTA, web serial...)
  EDControllerWebServer.loop();
  // handle rotary controllers
  encoder1->Handle();
  encoder2->Handle();
  encoder3->Handle();
}
