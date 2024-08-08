#include "I2CDeviceComMcu.h"
#include <EDIpcProtocolMaster.h>
#include <Logger.h>
#include <CrcParameters.h>
#include <CRC8.h>
// #include <OTAUpdate.h>

// OTAUpdate *otaUpdate = new OTAUpdate();

volatile bool _hasComMcuInterrupt = false;
unsigned long lastCheck = 0;

void IRAM_ATTR _comMcuInterruptHandler()
{
    _hasComMcuInterrupt = true;
}

I2CDeviceComMcu::I2CDeviceComMcu(const char *name, uint8_t interruptPin) : I2CDevice(0x12, name, interruptPin)
{
}

bool I2CDeviceComMcu::Init()
{
    if (IsPresent)
    {
        char buffer[100];
        snprintf_P(buffer, 99, (const char *)F("Init : %s, interrupt pin %d"), Name, InterruptPin);
        Logger.Log(buffer);
        pinMode(InterruptPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(InterruptPin), _comMcuInterruptHandler, FALLING);
    }
    return false;
}

uint8_t I2CDeviceComMcu::Handle()
{
    uint8_t changes = 0;
    if (_hasComMcuInterrupt || (millis() - lastCheck > 2000))
    {
        changes = EDIpcProtocolMaster::instance->retrieveChanges();
        // if (changes & UPDATE_CATEGORY::WIFI_CREDS)
        // {
        //     // if (otaUpdate->_active)
        //     //     otaUpdate->stop();
        //     // if (strlen(EDIpcProtocolMaster::instance->wifiCredentials.ssid) > 0)
        //     //     otaUpdate->begin("edcontrolpanel", EDIpcProtocolMaster::instance->wifiCredentials.ssid, EDIpcProtocolMaster::instance->wifiCredentials.password);
        // }        
        // char buffer[100];
        // snprintf_P(buffer, 99, (const char *)F("MCU get changes : %d"), changes);
        // Logger.Log(buffer);
        _hasComMcuInterrupt = false;
        lastCheck = millis();
   }
    EDIpcProtocolMaster::instance->sendChanges();
    return changes;
}

COM_REQUEST_TYPE mesId;
uint8_t mockChunkId;

bool I2CDeviceComMcu::sendMessageData(uint8_t messageId, uint8_t *messageData, size_t dataSize, uint8_t chunkId)
{
    mesId = static_cast<COM_REQUEST_TYPE>(messageId);
    //if ( mesId != COM_REQUEST_TYPE::CRT_MOCK)
        return I2CDevice::sendMessageData(messageId, messageData, dataSize, chunkId);

    // mockChunkId = chunkId;
    // return true;
}

uint8_t responseBuffer[I2C_MAX_MESSAGE_SIZE];
uint8_t responseSize;
CRC8 sendcrc;
uint8_t crcs[8];
uint8_t chunkcount = 0;

uint8_t I2CDeviceComMcu::requestData(uint8_t *receiveBuffer, size_t receiveBufferSize, bool strictSize)
{
    //if ( mesId != COM_REQUEST_TYPE::CRT_MOCK)
        return I2CDevice::requestData(receiveBuffer, receiveBufferSize, strictSize);
        
    if (mockChunkId == 0)
    {
        Serial.println("Received mock request, preparing response");
        sprintf((char *)responseBuffer, "LocationSystemName\tLocationStationName\tLocalAllegiance\tSystemSecurity\tNavroute1\tNavroute2\tNavroute3");
        responseSize = strlen((char *)responseBuffer) + 1;
        chunkcount = (responseSize + (I2C_CHUNK_SIZE-1)) / I2C_CHUNK_SIZE;
        Serial.printf("Response : %s, size %d, chunkcount %d\n", responseBuffer, responseSize, chunkcount);
        sendcrc.restart();
        sendcrc.setInitial(CRC8_INITIAL);
        uint8_t chunkId = 0;
        // calc crc for each chunk
        for (uint8_t i = 0; i < responseSize; i++)
        {
            sendcrc.add(responseBuffer[i]);
            if ((((i + 1) % I2C_CHUNK_SIZE) == 0) || (i == (responseSize - 1)))
            {
                crcs[chunkId] = sendcrc.calc();
                sendcrc.restart();
                sendcrc.setInitial(crcs[chunkId]);
                Serial.printf("byte %d, crc#%d = %d\n", i, chunkId, crcs[chunkId]);
                chunkId++;
            }                        
        }
                
    }

    if (mockChunkId >= chunkcount) return 0;

    memset(receiveBuffer, 0, receiveBufferSize);
    receiveBuffer[0] = 255;
    if (mockChunkId == 0)
        receiveBuffer[1] = chunkcount;
    else
        receiveBuffer[1] = mockChunkId;
    
    receiveBuffer[2] = crcs[mockChunkId];

    uint8_t chunkSize = (mockChunkId < (chunkcount - 1)) ? I2C_CHUNK_SIZE :  responseSize % I2C_CHUNK_SIZE;

    memcpy(receiveBuffer + 3, (void *)(responseBuffer + (mockChunkId * I2C_CHUNK_SIZE)), chunkSize);
    Serial.printf("Sending [0]=%d, [1]=%d, [2]=%d, size=%d, data=%s\n", receiveBuffer[0], receiveBuffer[1], receiveBuffer[2], chunkSize, receiveBuffer+3);
    return chunkSize + 3;
}