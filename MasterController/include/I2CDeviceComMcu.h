#pragma once

#include <I2CDevice.h>
#include <EDIpcProtocolMaster.h>

class I2CDeviceComMcu : public I2CDevice
{
public:
    I2CDeviceComMcu(const char *name, uint8_t interruptPin);
    bool sendMessageData(uint8_t messageId, uint8_t* messageData = nullptr, size_t dataSize = 0, uint8_t chunkId = CHUNKID_IGNORE) override;
    uint8_t requestData(uint8_t *receiveBuffer, size_t receiveBufferSize, bool strictSize = false) override;

    virtual bool Init();
    uint32_t Handle();

    // See wire.endTransmission for standard Errors.
    // supplemental errors :
    // 16: Data requested, but device return data not available
    // 17: Response data size mismatch
};
