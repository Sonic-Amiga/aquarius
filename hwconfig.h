#ifndef HWCONFIG_H
#define HWCONFIG_H

#include <map>
#include <string>
#include <vector>

#include "i2c_hw.h"

class HWConfig
{
public:
    HWConfig();
    ~HWConfig();

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

    void AddLeakDetector(const char* name, Switch* hw, const char* description)
    {
        hw->m_name = name;
        hw->m_description = description;
        m_LeakDetectors.push_back(hw);
    }

    I2C_pcf857x* m_ioExt1;
    std::map<std::string, Hardware*> m_hw;
    std::vector<Switch*> m_LeakDetectors;
};

#endif
