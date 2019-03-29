#include <mutex>
#include <string>
#include <vector>

#include "hwconfig.h"
#include "userdb.h"

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

    state_t GetState()
    {
        return m_state;
    }

    int       SetState(state_t state, const std::string &user);
    ctlmode_t GetMode() {return m_mode; }
    void      SetMode(ctlmode_t mode, const std::string &user);

    LeakSensor::status_t GetLeakState() {return m_LeakSensor->GetState();}
    int                  SetLeakState(LeakSensor::status_t, const std::string &user);
    int                  GetHeaterState() {return m_Heater->GetState();}
    int                  SetHeaterState(int state, const std::string &user);

    void HeaterWash(bool on);
    static bool IsFinalState(state_t);

    bool InFinalState()
    {
        return IsFinalState(m_state);
    }

    int ValveControl(const char* id, int& state, const std::string& user);
    int RelayControl(const char* id, bool& state, const std::string& user);

private:
    struct SavedState
    {
        int CalcCheck()
        {
            return ((int)State + (int)Mode) ^ 0x55AA55AA;
        }

        state_t   State;
        ctlmode_t Mode;
        int       Check;
    };

    bool LoadState();
    bool SaveState(state_t state, ctlmode_t mode);
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

    std::mutex        m_Lock;
};
