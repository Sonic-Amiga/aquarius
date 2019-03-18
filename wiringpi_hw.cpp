#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#include "logging.h"
#include "wiringpi_hw.h"

static void WPISetup(void)
{
    static bool didSetup;

    if (!didSetup) {
        wiringPiSetup();
        didSetup = true;
    }
}

WPIRelay::WPIRelay(int pin, bool resetState)
     : Relay(resetState), m_Pin(pin)
{
    WPISetup();

    digitalWrite(pin, resetState);
    pinMode(pin, OUTPUT);
}

void WPIRelay::ApplyState(bool on)
{
    digitalWrite(m_Pin, on);
}

WPII2C::WPII2C(int addr)
{
    m_fd = wiringPiI2CSetup(addr);

    if (m_fd == -1) {
        Log(Log::ERROR) << "Failed to connect to i2c address " << std::hex << addr;
    }
}

WPII2C::~WPII2C()
{
    if (m_fd != -1) {
        close(m_fd);
    }
}

bool WPII2C::Read(void* data, unsigned int size)
{
    if (m_fd != - 1) {
        return read(m_fd, data, size) == size;
    } else {
        return false;
    }
}

bool WPII2C::Write(void* data, unsigned int size)
{
    if (m_fd != - 1) {
        return write(m_fd, data, size) == size;
    } else {
        return false;
    }}
