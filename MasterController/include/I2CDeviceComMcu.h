#pragma once

#include <I2CDevice.h>
#include <EDIpcProtocolMaster.h>

class I2CDeviceComMcu : public I2CDevice
{
public:
    I2CDeviceComMcu(const char *name, uint8_t interruptPin);
    virtual bool Init();
    void Handle();

    // See wire.endTransmission for standard Errors.
    // supplemental errors :
    // 16: Data requested, but device return data not available
    // 17: Response data size mismatch
};
