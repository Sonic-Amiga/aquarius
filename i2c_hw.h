#ifndef I2C_HW_H
#define I2C_HW_H

#include "hardware.h"

class I2CPort
{
public:
    virtual bool Read(void* data, unsigned int size) {return true; }
    virtual bool Write(void* data, unsigned int size) {return true; }
};

class I2C_pcf857x
{
public:

    I2C_pcf857x(I2CPort* port, unsigned int nBits);
    ~I2C_pcf857x();
    int ReadBit(int bit, bool activeLow);
    void WriteBit(int bit, bool state);

    Switch* newSwitch(int bit, bool activeLow)
    {
        return new pcfSwitch(this, bit, activeLow);
    }

private:
    I2CPort* m_Port;
    unsigned int m_DataSize;
    unsigned int m_State;

    class pcfSwitch : public Switch
    {
    public:
        pcfSwitch(I2C_pcf857x* dev, int bit, bool activeLow)
            : Switch(activeLow), m_Dev(dev), m_Bit(bit)
        {
            dev->WriteBit(bit, true);
        }

        virtual int GetState()
        {
            return m_Dev->ReadBit(m_Bit, m_activeLow);
        }

    private:
        I2C_pcf857x* m_Dev;
        int m_Bit;
    };

};

#endif
