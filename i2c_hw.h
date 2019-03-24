#ifndef I2C_HW_H
#define I2C_HW_H

#include "hardware.h"
#include "hwconfig.h"

class I2CPort
{
public:
    virtual bool Read(void* data, unsigned int size) = 0;
    virtual bool Write(void* data, unsigned int size) = 0;
};

class I2CBus : public Hardware
{
public:
    virtual I2CPort *CreatePort(unsigned int addr) = 0;
    I2CPort *CreatePort(xmlNode *node);
};

class PCF857x : public Hardware
{
public:
    PCF857x(I2CPort* port, unsigned int nBits);
    ~PCF857x();
    int ReadBit(int bit, bool activeLow);
    void WriteBit(int bit, bool state);

    Switch* newSwitch(int bit, bool activeLow);

private:
    I2CPort* m_Port;
    unsigned int m_DataSize;
    unsigned int m_State;
};

class PCFSwitch : public Switch
{
public:
    PCFSwitch(PCF857x* dev, int bit, bool activeLow)
        : Switch(activeLow), m_Dev(dev), m_Bit(bit)
    {
        dev->WriteBit(bit, true);
    }

    virtual int GetState()
    {
        return m_Dev->ReadBit(m_Bit, m_activeLow);
    }

private:
    PCF857x* m_Dev;
    int m_Bit;
};

#endif
