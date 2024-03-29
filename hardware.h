#ifndef HARDWARE_H
#define HARDWARE_H

#include <time.h>
#include <string>

#include "event_bus.h"

class Hardware
{
public:
    virtual ~Hardware() {}

    virtual void ReportCurrentState() const {}

    std::string m_name;
    std::string m_description;

protected:
    void ReportState(const std::string& prefix, int value) const
    {
        if (!m_name.empty())
            SendEvent(prefix + '/' + m_name + "/state", value);
    }

    void ReportValue(const std::string& prefix, float value) const
    {
        if (!m_name.empty())
            SendEvent(prefix + '/' + m_name + "/value", value);
    }
};

class Relay : public Hardware
{
public:
    static const char* const statusStrings[];

    Relay(bool resetState)
        : m_State(false), m_ResetState(resetState) {}

    void SetState(bool st)
    {
        ReportState(st);
        ApplyState(st ? !m_ResetState : m_ResetState);
    }

    bool GetState()
    {
        return m_State;
    }

    void ReportCurrentState() const override
    {
        Hardware::ReportState("relay", m_State);
    }

protected:
    virtual void ApplyState(bool on) = 0;

private:
    void ReportState(bool state)
    {
        m_State = state;
        ReportCurrentState();
    }

    bool m_State;
    bool m_ResetState;
};

class Switch : public Hardware
{
public:
    enum // States
    {
        Off,
        On,
        Fault
    };

    Switch(bool inverted) : m_activeLow(inverted), m_LastState(Off)
    {}

    virtual int GetState() = 0;

    int poll()
    {
        int state = GetState();

        if (state != m_LastState) {
            m_LastState = state;
            ReportCurrentState();
        }

        return state;
    }

    void SetStatePrefix(const char* s)
    {
        m_StatePrefix = s;
    }

    void ReportCurrentState() const override
    {
        Hardware::ReportState(m_StatePrefix, m_LastState);
    }


protected:
    bool m_activeLow;

private:
    int         m_LastState;
    std::string m_StatePrefix = "switch";
};

class Thermometer : public Hardware
{
public:
    enum // States
    {
        Fault,
        Cold,
        Normal
    };

    Thermometer(float thresh) : m_State(Normal), m_Threshold(thresh), m_LastValue(0)
    {}

    virtual int GetState()
    {
        return m_State;
    }

    float GetValue();

    void ReportCurrentState() const override
    {
        Hardware::ReportState("thermometer", m_State);
        Hardware::ReportValue("thermometer", m_LastValue);
    }

protected:
    virtual float Measure()
    {
        return 0.0;
    }

    float m_Threshold;

private:
    int   m_State;
    float m_LastValue;
};

class Valve : public Hardware
{
public:
    enum // States
    {
        Reset,
        Closed,
        Closing,
        Opening,
        Open,
        Fault
    };

    static const char* const statusStrings[];

    Valve(time_t timeout,
          Relay* openPin, Relay* closePin,
          Switch* openSwitch, Switch* closeSwitch);
    virtual ~Valve();

    void Poll();
    bool SetState(int state, bool force = false);
    int  GetState() {return m_State;}

    void ReportCurrentState() const {
        Hardware::ReportState("valve", m_State);
    }

private:
    void ReportFault(const char* s);
    void GetStateFromSwitches();

    bool HaveSwitches()
    {
	return m_OpenSwitch && m_CloseSwitch;
    }

    void ReportState(int state)
    {
        m_State = state;
        ReportCurrentState();
    }

    int    m_State;
    time_t m_StateChange;
    time_t m_StateChangeTimeout;

    Relay*  m_OpenPin;
    Relay*  m_ClosePin;
    Switch* m_OpenSwitch;
    Switch* m_CloseSwitch;
};

#endif
