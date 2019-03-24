#ifndef HWCONFIG_H
#define HWCONFIG_H

#include <libxml/tree.h>
#include <map>
#include <string>
#include <vector>

#include "hardware.h"

class PCF857x;

static inline const char *GetStrProp(xmlNode *node, const char *name)
{
    return (const char *)xmlGetProp(node, (const xmlChar *)name);
}

int GetIntProp(xmlNode *node, const char *name);
float GetFloatProp(xmlNode *node, const char *name);

class HWConfig
{
public:
    HWConfig();
    ~HWConfig();

    Hardware *GetDeviceProp(xmlNode *node, const char *name);
    Hardware *GetParentHW()
    {
        return m_Parent;
    }

    template<class T>
    std::vector<T*> GetHWList()
    {
        std::vector<T*> ret;

        for (auto& hw : m_hw) {
            T* typedHW = dynamic_cast<T*>(hw.second);

            if (typedHW)
                ret.push_back(typedHW);
        }

        return ret;

    }

    template<class T>
    T* GetHardware(const char* name)
    {
        return dynamic_cast<T*>(m_hw[name]);
    }

    const std::vector<Valve*> GetValves()
    {
        return GetHWList<Valve>();
    }

    const std::vector<Relay*> GetRelays()
    {
        return GetHWList<Relay>();
    }

    const std::vector<Switch*> GetSwitches()
    {
        return GetHWList<Switch>();
    }

    const std::vector<Switch*>& GetLeakDetectors()
    {
        return m_LeakDetectors;
    }

private:
    void AddHardware(const char* name, Hardware* hw, const char* description)
    {
        hw->m_name = name;
        hw->m_description = description;
        m_hw[name] = hw;
    }

    void AddLeakSensor(Switch* hw)
    {
        m_LeakDetectors.push_back(hw);
    }

    void AddHardware(Hardware* hw)
    {
        if (hw->m_name.empty())
            m_AnonHW.push_back(hw);
        else
            m_hw[hw->m_name] = hw;
    }

    void AddLeakDetector(const char* name, Switch* hw, const char* description)
    {
        hw->m_name = name;
        hw->m_description = description;
        m_LeakDetectors.push_back(hw);
    }

    Hardware *createDevice(xmlNode *node);
    void readNodes(xmlNode *startNode, const char *name, void(HWConfig::*parserFunc)(xmlNode *));
    void createBus(xmlNode *node);
    void createDeviceOnBus(xmlNode *node);
    void createHeater(xmlNode *node);
    void createLeakDetector(xmlNode *node);
    void createValveController(xmlNode *node);
    Valve *createValve(xmlNode *node);

    Hardware *m_Parent;

    PCF857x* m_ioExt1;
    std::map<std::string, Hardware*> m_hw;
    std::vector<Switch*> m_LeakDetectors;
    std::vector<Hardware *>m_AnonHW;
};

class DeviceType
{
public:
    DeviceType(const char* type);
    virtual Hardware *CreateDevice(xmlNode *, HWConfig *) = 0;

    const char *m_Type;
    DeviceType *m_Next;
};

/*
 * This macro defines a metaclass, which allows the XML deserializer to look up
 * the device class by name and instantiate it.
 * Class-specific deserialization is performed by CreateDevice() method, which
 * you define when using this macro. See source code for examples.
 */
#define REGISTER_DEVICE_TYPE(name)					\
class name ## _Factory : public DeviceType				\
{									\
public:									\
    name ## _Factory() : DeviceType( #name ) {}				\
    virtual Hardware *CreateDevice(xmlNode *, HWConfig *) override;	\
};									\
static name ## _Factory name ## _type;					\
Hardware * name ## _Factory::CreateDevice

#endif
