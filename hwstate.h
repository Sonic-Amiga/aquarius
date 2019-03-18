#include <pthread.h>

#include <string>
#include <vector>

#include "hwconfig.h"

class LeakSensor
{
public:
    typedef enum
    {
        Fault,
        Enabled,
        Disabled,
        Alarm
    } status_t;

    LeakSensor(HWConfig* cfg);
    ~LeakSensor()
    {
        delete[] m_SensorState;
    }

    status_t GetState() {return m_state;}
    void     SetState(status_t state) {m_state = state;}

    bool Poll();

private:
    std::vector<Switch*> m_Sensors;
    int*                 m_SensorState;
    status_t             m_state;
};

class HWState;

class HeaterController
{
public:
    enum
    {
        Fault,
        OK,
        Wash,
        Protection,
        Pressurize,
    };

    HeaterController(HWState* hw, HWConfig* cfg);

    void SetState(int state);
    int GetState();

    void Poll(int s_HI);
    void Control(bool on);

private:
    typedef enum
    {
        None,    // Idle
        Pending, // Wash requested
        Fill,    // Open heater input valve
        Drain,   // Open drain valve, wait
        Refill   // Close drain valve, repressurize the heater
    } WashStep;

    void ApplyState(int state);
    void StartWash();
    void EndWash();

    HWState* m_HW;

    Relay * m_Heater;
    Relay * m_Drain;
    Switch* m_Pressure;

    int      m_State;
    WashStep m_washStep;
    time_t   m_washTimer;
};

class HWState
{
public:
    typedef enum
    {
        Fault,
        Closing,
        Closed,
        SwitchToCentral,
        Central,
        SwitchToHeater,
        Heater,
        Maintenance
    } state_t;

    typedef enum
    {
        BadMode = -1,
        Auto,
        Manual,
        FullManual // Maintenance
    } ctlmode_t;

    HWState(HWConfig* cfg);
    ~HWState();

    void Poll();

    state_t             GetState();
    int                 SetState(state_t state);
    ctlmode_t           GetMode() {return m_mode; }
    void                SetMode(ctlmode_t mode);

    LeakSensor::status_t GetLeakState() {return m_LeakSensor->GetState();}
    int                  SetLeakState(LeakSensor::status_t);
    int                  GetHeaterState() {return m_Heater->GetState();}
    int                  SetHeaterState(int state);

    void HeaterWash(bool on);
    bool InFinalState();

    int ValveControl(const char* id, int& state);
    int RelayControl(const char* id, bool& state);

private:
    void ApplyState(state_t state);

    HWConfig* m_Cfg;

    Valve* m_CS;
    Valve* m_HS;
    Valve* m_HI;
    Valve* m_HO;

    LeakSensor      * m_LeakSensor;
    HeaterController* m_Heater;

    state_t           m_state;
    unsigned int      m_step;      // State transition step
    ctlmode_t         m_mode;

    pthread_mutex_t m_Lock;
};
