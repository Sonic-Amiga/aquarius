#include "hardware.h"
#include "i2c_hw.h"

class WPIRelay : public Relay
{
public:
    WPIRelay(int pin, bool resetState);

protected:
    virtual void ApplyState(bool on) override;

private:
    int m_Pin;
};

class WPII2CPort : public I2CPort
{
public:
    WPII2CPort(int addr);
    virtual ~WPII2CPort();
    virtual bool Read(void* data, unsigned int size) override;
    virtual bool Write(void* data, unsigned int size) override;

private:
    int m_fd;
};

class WPII2C : public I2CBus
{
public:
    virtual I2CPort *CreatePort(unsigned int addr) override
    {
        return new WPII2CPort(addr);
    }
};
