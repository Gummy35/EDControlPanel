
#include <Arduino.h>

#include <EDIpcProtocolSlave.h>
#include <EDSerialProtocol.h>


#define SDA1 SDA // 2
#define SCL1 SCL // 3
#define SIGNAL_MASTER_PIN PIND4 // 9 // 0 1 2 3 7

EDIpcProtocolSlave* IpcProtocolInstance;
EDSerialProtocol* SerialProtocolInstance;


int GetFreeRam() {
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0  
    ? (int)&__heap_start : (int) __brkval);  
}

void DisplayFreeram() {
  Serial.print(F("L\tSRAM left: "));
  Serial.println(GetFreeRam());
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(0x12);
  Wire.setClock(400000);

  IpcProtocolInstance = new EDIpcProtocolSlave(&Wire, SIGNAL_MASTER_PIN);
  SerialProtocolInstance = new EDSerialProtocol(IpcProtocolInstance);
  pinMode(SIGNAL_MASTER_PIN, OUTPUT);
  digitalWrite(SIGNAL_MASTER_PIN, HIGH);
  
  IpcProtocolInstance->begin();
  SerialProtocolInstance->Begin();
}


unsigned long lasttick = 0;
void loop()
{
  SerialProtocolInstance->Handle();
  IpcProtocolInstance->updateDevices();
  delay(5);
  // if (millis() - lasttick > 30000)
  // {
  //   DisplayFreeram();
  //   lasttick = millis();
  // }
}