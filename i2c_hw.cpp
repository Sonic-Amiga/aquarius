#include <endian.h>

#include "i2c_hw.h"

I2C_pcf857x::I2C_pcf857x(I2CPort* port, unsigned int nBits) : m_Port(port), m_DataSize(nBits / 8)
{
    unsigned int buf = 0;

    m_Port->Read(&buf, m_DataSize);
    m_State = le32toh(buf);
}

I2C_pcf857x::~I2C_pcf857x()
{
    delete m_Port;
}

int I2C_pcf857x::ReadBit(int bit, bool activeLow)
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

void I2C_pcf857x::WriteBit(int bit, bool state)
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
