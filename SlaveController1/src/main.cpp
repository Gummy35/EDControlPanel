const char  infoString []   = "EDControlPanel V1.0.0";
#include <Arduino.h>

#include <EDIpcProtocolSlave.h>
#include <EDGameVariables.h>
#include <Keyboard.h>

#define SDA1 SDA // 2
#define SCL1 SCL // 3
#define SIGNAL_MASTER_PIN PIND4 // 9 // 0 1 2 3 7

EDIpcProtocolSlave* _EDIpcProtocol;


char * currentStarSystem;
int hasConfigChange = 0;
int requestSerial = -1;
volatile char * currentStatus;

boolean outputUI = false;
boolean startup = true;

#if defined(ESP8266) || defined(ESP32)

extern "C" char* sbrk(int incr);
int GetFreeRam() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}

#else

int GetFreeRam() {
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0  
    ? (int)&__heap_start : (int) __brkval);  
}

#endif

void DisplayFreeram() {
  Serial.print(F("L\tSRAM left: "));
  Serial.println(GetFreeRam());
}

void setup()
{
  //while (!Serial) delay(10);
  Serial.begin(115200);
  Wire.begin(0x12);
  Wire.setClock(400000);

  _EDIpcProtocol = new EDIpcProtocolSlave(&Wire, SIGNAL_MASTER_PIN);

  currentStarSystem = (char *)malloc(21);
  memset(currentStarSystem, 0, 21);
  pinMode(SIGNAL_MASTER_PIN, OUTPUT);
  digitalWrite(SIGNAL_MASTER_PIN, HIGH);
  // event handler initializations
  
  _EDIpcProtocol->begin();

  startup = false;
}

/// @brief Serial command I
void sendInfo()
{
  Serial.print("I\t");
  Serial.println(infoString);
}

void readSerialLine(char * res, int maxsize, char separator)
{
  static char buffer[100 + 1] = {0};
  static size_t size = 0;
  size = Serial.readBytesUntil(separator, buffer, 100);
  buffer[size] = 0;
  if (res != nullptr)
    strncpy(res, buffer, maxsize);
}

void readSeparator()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if ((c == '\t') || (c == 0))
      return;
  }
}

void parseInput()
{
  while (Serial.available() > 0)
  {
    // read the incoming byte:
    byte command = Serial.read();

    if (command == 'C')
    {
      DisplayFreeram();
      //Keyboard.println("Updating data");
      byte len, data;
      memset(currentStarSystem, (byte)' ', 20);
      len = Serial.read();
      for (int i = 0; i < len; i++)
      {
        data = Serial.read();
        if (i<20) {
          currentStarSystem[i] = (char)data;
        }
      }
      hasConfigChange = 1;
      digitalWrite(SIGNAL_MASTER_PIN, LOW);
      delay(10);
      digitalWrite(SIGNAL_MASTER_PIN, HIGH);
      DisplayFreeram();
    } 
    else if (command == SERIAL_COMMAND_SILENT)
    {
      outputUI = false;
      Serial.println("S"); //silent
    }
    else if (command == SERIAL_COMMAND_HELLO)
    {
      if (startup)
        Serial.println("h");
      else
        Serial.println("H"); // Hello
    }
    else if (command == SERIAL_COMMAND_VERBOSE)
    {
      Serial.println("V"); //verbose
      sendInfo();
      outputUI = true;
    }
    else if (command == SERIAL_COMMAND_SENDINFOS)
    {
      sendInfo();
    }
    else if (command == SERIAL_COMMAND_LOCATION)
    {
      readSeparator();
      readSerialLine(EDGameVariables.LocationSystemName, 20, '\t');
      readSerialLine(EDGameVariables.LocationStationName, 20, '\t');
      readSerialLine(EDGameVariables.LocalAllegiance, 20, '\t');
      readSerialLine(EDGameVariables.SystemSecurity, 20, '\0');
      Serial.print("l\tEDGameVariables.LocationSystemName:");
      Serial.print(EDGameVariables.LocationSystemName);
      Serial.print("\tEDGameVariables.LocationStationName:");
      Serial.print(EDGameVariables.LocationStationName);
      Serial.print("\tEDGameVariables.LocalAllegiance:");
      Serial.print(EDGameVariables.LocalAllegiance);
      Serial.print("\tEDGameVariables.SystemSecurity:");
      Serial.println(EDGameVariables.SystemSecurity);
      Serial.print("L\told update flags: ");
      Serial.print(_EDIpcProtocol->_updateFlag, 2);
      _EDIpcProtocol->addUpdate(UPDATE_CATEGORY::UC_LOCATION);
      Serial.print(" new flag : ");
      Serial.println(_EDIpcProtocol->_updateFlag, 2);
      _EDIpcProtocol->signalMaster();
    }
    else if (command == SERIAL_COMMAND_GAMEINFOS)
    {
      readSeparator();
      readSerialLine(EDGameVariables.InfosCommanderName, 20, '\t');
      readSerialLine(EDGameVariables.InfosShipName, 20, '\0');
      Serial.print("g\t");
      Serial.print(EDGameVariables.InfosCommanderName);
      Serial.print("\t");
      Serial.println(EDGameVariables.InfosShipName);
      Serial.print("L\told update flags: ");
      Serial.print(_EDIpcProtocol->_updateFlag, 2);
      _EDIpcProtocol->addUpdate(UPDATE_CATEGORY::UC_INFOS);
      Serial.print(" new flag : ");
      Serial.println(_EDIpcProtocol->_updateFlag, 2);
      _EDIpcProtocol->signalMaster();
    }
    else if (command == SERIAL_COMMAND_GAMEFLAGS)
    {
      Serial.readBytes((uint8_t *)(&EDGameVariables.StatusFlags1), 4);
      Serial.readBytes((uint8_t *)(&EDGameVariables.StatusFlags2), 4);
      Serial.readBytes((uint8_t *)(&EDGameVariables.GuiFocus), 4);
      readSerialLine(EDGameVariables.StatusLegal, 20, '\0');
      Serial.print("f\t");
      Serial.println(EDGameVariables.StatusLegal);
      Serial.print("L\told update flags: ");
      Serial.print(_EDIpcProtocol->_updateFlag, 2);
      _EDIpcProtocol->addUpdate(UPDATE_CATEGORY::UC_STATUS);
      Serial.print(" new flag : ");
      Serial.println(_EDIpcProtocol->_updateFlag, 2);
      _EDIpcProtocol->signalMaster();
    }
    else if (command == SERIAL_COMMAND_LOADOUT)
    {
      Serial.readBytes((uint8_t *)(&EDGameVariables.LoadoutFlags1), 4);
      Serial.readBytes((uint8_t *)(&EDGameVariables.LoadoutFlags2), 4);
      readSerialLine(nullptr, 20, '\0');
      _EDIpcProtocol->addUpdate(UPDATE_CATEGORY::UC_STATUS);
      _EDIpcProtocol->signalMaster();
    }
    else if (command == SERIAL_COMMAND_NAVROUTE)
    {
      readSeparator();
      readSerialLine(EDGameVariables.Navroute1, 20, '\t');
      readSerialLine(EDGameVariables.Navroute2, 20, '\t');
      readSerialLine(EDGameVariables.Navroute3, 20, '\0');
      Serial.print("n\t");
      Serial.print(EDGameVariables.Navroute1);
      Serial.print("\t");
      Serial.print(EDGameVariables.Navroute2);
      Serial.print("\t");
      Serial.println(EDGameVariables.Navroute3);
      Serial.print("L\told update flags: ");
      Serial.print(_EDIpcProtocol->_updateFlag, 2);
      _EDIpcProtocol->addUpdate(UPDATE_CATEGORY::UC_LOCATION);
      Serial.print(" new flag : ");
      Serial.println(_EDIpcProtocol->_updateFlag, 2);
      _EDIpcProtocol->signalMaster();
    }
    else if (command == SERIAL_COMMAND_ALERTS)
    {
      readSeparator();
      readSerialLine(EDGameVariables.AlertMessage1, 20, '\t');
      readSerialLine(EDGameVariables.AlertMessage2, 20, '\t');
      readSerialLine(EDGameVariables.AlertMessage3, 20, '\0');
      Serial.print("L\tnew alert message:");
      Serial.print("\t");
      Serial.print(EDGameVariables.AlertMessage1);
      Serial.print("\t");
      Serial.print(EDGameVariables.AlertMessage2);
      Serial.print("\t");
      Serial.println(EDGameVariables.AlertMessage3);
      _EDIpcProtocol->addUpdate(UPDATE_CATEGORY::UC_URGENT_INFO);
      _EDIpcProtocol->signalMaster();
    }
  }
}

unsigned long lasttick = 0;
void loop()
{
  parseInput();
  _EDIpcProtocol->updateDevices();
  delay(5);
  if (millis() - lasttick > 30000)
  {
    DisplayFreeram();
    lasttick = millis();
  }
}