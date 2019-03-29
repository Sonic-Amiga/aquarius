#include <stdio.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#include "hwconfig.h"
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


WPII2CPort::WPII2CPort(int addr)
{
    m_fd = wiringPiI2CSetup(addr);

    if (m_fd == -1) {
        Log(Log::ERR) << "Failed to connect to i2c address " << std::hex << addr;
    }
}

WPII2CPort::~WPII2CPort()
{
    if (m_fd != -1) {
        close(m_fd);
    }
}

bool WPII2CPort::Read(void* data, unsigned int size)
{
    if (m_fd != - 1) {
        return read(m_fd, data, size) == size;
    } else {
        return false;
    }
}

bool WPII2CPort::Write(void* data, unsigned int size)
{
    if (m_fd != - 1) {
        return write(m_fd, data, size) == size;
    } else {
        return false;
    }
}

// *** XML deserializers begin here ***

REGISTER_DEVICE_TYPE(WPIRelay)(xmlNode *node, HWConfig *)
{
    int pin = GetIntProp(node, "pin");
    int inactive = GetIntProp(node, "inactive");

    if ((pin == -1) || (inactive == -1)) {
        Log(Log::ERR) << "Malformed WPIRelay description";
        return nullptr;
    } else {
        return new WPIRelay(pin, inactive);
    }
}

REGISTER_DEVICE_TYPE(WPII2C)(xmlNode *node, HWConfig *)
{
    return new WPII2C();
}
