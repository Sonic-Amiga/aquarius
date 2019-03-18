#include "linux_hw.h"

LinuxI2C::LinuxI2C(const char* port, int addr)
{
    m_fd = open (device, O_RDWR);

    if (m_fd == -1)
        return;

    if (ioctl (fd, I2C_SLAVE, devId) < 0) {
        close(m_fd);
        m_fd = -1;
    }
}

LinuxI2C::~LinuxI2C()
{
    close(m_fd);
}

unsigned int LinuxI2C::Read()
{
    unsigned int ret = wiringPiI2CRead(m_fd);

    return ret;
}

void LinuxI2C::Write(unsigned int data)
{
    wiringPiI2CWrite(m_fd, data);
}
