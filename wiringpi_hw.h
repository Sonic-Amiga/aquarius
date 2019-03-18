#include "hardware.h"
#include "i2c_hw.h"

class WPIRelay : public Relay
{
public:
    WPIRelay(int pin, bool resetState);

protected:
    virtual void ApplyState(bool on);

private:
    int m_Pin;
};

class WPII2C : public I2CPort
{
public:
    WPII2C(int addr);
    virtual ~WPII2C();
    virtual bool Read(void* data, unsigned int size);
    virtual bool Write(void* data, unsigned int size);

private:
    int m_fd;
};
