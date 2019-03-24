#ifndef HARDWARE_H
#define HARDWARE_H

#include <pthread.h>
#include <time.h>

class Hardware
{
public:
    virtual ~Hardware() {}

    std::string m_name;
    std::string m_description;
};

class Relay : public Hardware
{
public:
    static const char* const statusStrings[];

    Relay(bool resetState)
        : m_State(false), m_ResetState(resetState) {}

    void SetState(bool st)
    {
        m_State = st;
        ApplyState(st ? !m_ResetState : m_ResetState);
    }

    bool GetState()
    {
        return m_State;
    }

protected:
    virtual void ApplyState(bool on) {}

private:
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

    Switch(bool inverted) : m_activeLow(inverted)
    {}

    virtual int GetState()
    {
        return Off;
    }

protected:
    bool m_activeLow;
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

    Thermometer(float thresh) : m_State(Normal), m_Threshold(thresh)
    {}

    virtual int GetState()
    {
        return m_State;
    }

    float GetValue();

protected:
    virtual float Measure()
    {
        return 0.0;
    }

    int m_State;
    float m_Threshold;
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

private:
    void ReportFault(const char* s);
    void GetStateFromSwitches();

    int    m_State;
    time_t m_StateChange;
    time_t m_StateChangeTimeout;

    Relay*  m_OpenPin;
    Relay*  m_ClosePin;
    Switch* m_OpenSwitch;
    Switch* m_CloseSwitch;
};

#endif
