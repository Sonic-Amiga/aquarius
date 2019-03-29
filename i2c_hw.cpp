#include "hwconfig.h"
#include "i2c_hw.h"
#include "logging.h"
#include "utils.h"

I2CPort* I2CBus::CreatePort(xmlNode *node)
{
    int addr = GetIntProp(node, "address");

    if (addr == -1) {
        Log(Log::ERR) << "Malformed I2C address in config";
        return nullptr;
    } else {
        return CreatePort(addr);
    }
}

PCF857x::PCF857x(I2CPort* port, unsigned int nBits) : m_Port(port), m_DataSize(nBits / 8)
{
    unsigned int buf = 0;

    m_Port->Read(&buf, m_DataSize);
    m_State = le32toh(buf);
}

PCF857x::~PCF857x()
{
    delete m_Port;
}

int PCF857x::ReadBit(int bit, bool activeLow)
{
    unsigned int buf = 0;

    if (m_Port->Read(&buf, m_DataSize)) {
        int val = le32toh(buf) & (1U << bit);

        if (activeLow)
            val = !val;

        return val ? Switch::On : Switch::Off;
    } else {
        return Switch::Fault;
    }
}

void PCF857x::WriteBit(int bit, bool state)
{
    unsigned int mask = 1U << bit;
    unsigned int buf;

    if (state)
        m_State |= mask;
    else
        m_State &= ~mask;

    buf = htole32(m_State);
    m_Port->Write(&buf, 2);
}

Switch* PCF857x::newSwitch(int bit, bool activeLow)
{
    return new PCFSwitch(this, bit, activeLow);
}

//*** XML deserializers begin here ***

REGISTER_DEVICE_TYPE(PCF857x)(xmlNode *node, HWConfig *cfg)
{
    I2CBus *bus = dynamic_cast<I2CBus *>(cfg->GetParentHW());

    if (!bus) {
        Log(Log::ERR) << "Incorrect bus type for PCF857x config";
        return nullptr;
    }

    I2CPort *port = bus->CreatePort(node);
    int pincnt = GetIntProp(node, "pincount");

    if (pincnt == -1) {
        Log(Log::ERR) << "Malformed PCF857x definition in config";
        delete port;
        return nullptr;
    }

    return new PCF857x(port, pincnt);
}

REGISTER_DEVICE_TYPE(PCFSwitch)(xmlNode *node, HWConfig *cfg)
{
    PCF857x *device = dynamic_cast<PCF857x *>(cfg->GetDeviceProp(node, "device"));
    int pin = GetIntProp(node, "pin");
    int inverted = GetIntProp(node, "inverted", 0);

    if ((!device) || (pin == -1) || (inverted == -1)) {
        Log(Log::ERR) << "Malformed PCFSwitch definition";
        return nullptr;
    } else {
        return new PCFSwitch(device, pin, inverted);
    }
}
