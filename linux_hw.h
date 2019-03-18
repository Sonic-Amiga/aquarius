#include "hardware.h"
#include "i2c_hw.h"

class LinuxI2C : public I2CPort
{
public:
    LinuxI2C(const char* dev, int addr);
    virtual ~LinuxI2C();
    virtual unsigned int Read();
    virtual void Write(unsigned int data);

private:
    int m_fd;
};
