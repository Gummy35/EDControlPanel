/* I2C slave Address Scanner
Kutscher07: Modified for TTGO TQ board with builtin OLED
Quelle: https://github.com/espressif/arduino-esp32/issues/977
*/

#include <Arduino.h>

#include <EDIpcProtocolSlave.h>
#include <Keyboard.h>

#define SDA1 SDA // 2
#define SCL1 SCL // 3
#define SIGNAL_MASTER_PIN PIND4 // 9 // 0 1 2 3 7

EDIpcProtocolSlave* _EDIpcProtocol;


char * currentStarSystem;
int hasConfigChange = 0;
int requestSerial = -1;
volatile char * currentStatus;




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
  Serial.print(F("- SRAM left: "));
  Serial.println(GetFreeRam());
}

void setup()
{
  //while (!Serial) delay(10);
  Serial.begin(115200);
  Wire.begin(0x12);
  Wire.setClock(400000);

  _EDIpcProtocol = new EDIpcProtocolSlave(&Wire);

  currentStarSystem = (char *)malloc(21);
  memset(currentStarSystem, 0, 21);
  pinMode(SIGNAL_MASTER_PIN, OUTPUT);
  digitalWrite(SIGNAL_MASTER_PIN, HIGH);
  // event handler initializations
  
  _EDIpcProtocol->begin();
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
  }
}

unsigned long n = 0;
void loop()
{
  parseInput();
  _EDIpcProtocol->updateDevices();
  delay(5);   
}