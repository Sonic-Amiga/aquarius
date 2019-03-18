#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "hwstate.h"
#include "logging.h"
#include "wiringpi_hw.h"

LeakSensor::LeakSensor(HWConfig* cfg) : m_state(Enabled)
{
    m_Sensors = cfg->GetLeakDetectors();
    m_SensorState = new int[m_Sensors.size()];

    for (int i = 0; i < m_Sensors.size(); i++)
        m_SensorState[i] = Switch::Off;
}


bool LeakSensor::Poll()
{
    bool alarm = false;

    for (int i = 0; i < m_Sensors.size(); i++) {
        Switch* s = m_Sensors[i];
        int ss = s->GetState();

        if (m_SensorState[i] == ss) {
            // Avoid flooding the log
            continue;
        }

        m_SensorState[i] = ss;

        switch (ss) {
        case Switch::On:
            Log(Log::WARN) << "Leak detected in " << s->m_description;
            if (m_state == Enabled) {
                m_state = Alarm;
                alarm = true;
            }
            break;

        case Switch::Fault:
            Log(Log::ERROR) << "Leak sensor fault in " << s->m_description;
            break;
        }
    }

    return alarm;
}

// TODO: This is configurable
static const int WashDelay   = 20;
static const int RefillDelay = 2;

HeaterController::HeaterController(HWState* hw, HWConfig* cfg)
    : m_HW(hw), m_State(OK), m_washStep(None)
{
    m_Heater   = cfg->GetHardware<Relay>("HR");
    m_Drain    = cfg->GetHardware<Relay>("HD");
    m_Pressure = cfg->GetHardware<Switch>("HP");
}

void HeaterController::SetState(int state)
{
    switch (state)
    {
    case Wash:
        m_washStep = WashStep::Pending;
        StartWash();
    }
}

void HeaterController::ApplyState(int state)
{
    m_State = state;

    switch (state)
    {
    case Fault:
    case Protection:
        EndWash();
        m_Heater->SetState(false);
        break;

    case Pressurize:
        if (m_HW->GetState() == HWState::Heater) {
            SetState(Wash);
        }
    };
}

int HeaterController::GetState()
{
    return m_State;
}

void HeaterController::Control(bool on)
{
    switch (m_State)
    {
    case OK:
        m_Heater->SetState(on);
        break;

    case Pressurize:
        if (on) {
            SetState(Wash);
        }
    }
}

void HeaterController::Poll(int s_HI)
{
    int s_HP = m_Pressure->GetState();

    if (s_HP == Switch::Fault) {
        if (m_State != Fault) {
            Log(Log::ERROR) << "Heater pressure monitor fault";
            ApplyState(Fault);
        }
    }
    else if (m_washStep == Pending)
    {
        // Attempt to start
        StartWash();
    }
    else if (m_washStep != None)
    {
        if (s_HI == Valve::Open)
        {
            if (m_washStep == WashStep::Fill) {
                m_Drain->SetState(true);
                m_washTimer = time(NULL) + WashDelay;
                m_washStep = WashStep::Drain;
            }
            if ((m_washStep == WashStep::Drain) && (time(NULL) > m_washTimer)) {
                m_Drain->SetState(false);
                m_washTimer = time(NULL) + RefillDelay;
                m_washStep = WashStep::Refill;
            }
            if (m_washStep == WashStep::Refill) {
                switch (s_HP)
                {
                case Switch::Off:
                    if (time(NULL) > m_washTimer) {
                        Log(Log::WARN) << "Heater failed to re-pressurize";
                        ApplyState(Protection);
                    }
                    break;

                case Switch::On:
                    EndWash();
                    ApplyState(OK);
                    break;
                }
            }
        } else if (s_HI != Valve::Opening) {
            Log(Log::WARN) << "Heater wash aborted";
            EndWash();
            m_State = Pressurize;
        }
    }
    else
    {
        if ((s_HP == Switch::Off) && (m_State != Protection))
        {
            Log(Log::WARN) << "Heater pressure lost";
            ApplyState(Protection);
        }
        else if ((s_HP == Switch::On) && (m_State == Protection))
        {
            Log(Log::INFO) << "Heater pressure restored";
            ApplyState(Pressurize);
        }
    }
}

void HeaterController::StartWash()
{
    if (m_HW->InFinalState())
    {
        m_HW->HeaterWash(true);
        m_State = Wash;
        m_washStep = Fill;
    }
}

void HeaterController::EndWash()
{
    if (m_washStep != None) {
        m_Drain->SetState(false);
        // After this HWState will call Control() if needed, so we don't have
        // to mess with heater relay here
        m_HW->HeaterWash(false);
        m_washStep = None;
    }
}

static const char *modeStrings[] =
{
    "Auto",
    "Manual",
    "Maintenance"
};

HWState::HWState(HWConfig* cfg)
    : m_Cfg(cfg), m_state(Maintenance), m_mode(Manual)
{
    pthread_mutex_init(&m_Lock, NULL);

    m_CS = cfg->GetHardware<Valve>("CS"); // Cold Supply
    m_HS = cfg->GetHardware<Valve>("HS"); // Hot Supply
    m_HI = cfg->GetHardware<Valve>("HI"); // Heater in
    m_HO = cfg->GetHardware<Valve>("HO"); // Heater out

    m_LeakSensor = new LeakSensor(cfg);
    m_Heater     = new HeaterController(this, cfg);
}

HWState::~HWState()
{
    delete m_LeakSensor;
    delete m_Heater;

    pthread_mutex_destroy(&m_Lock);
}

void HWState::Poll()
{
    pthread_mutex_lock(&m_Lock);

    if (m_LeakSensor->Poll()) {
        ApplyState(Closed);
    }

    m_CS->Poll();
    m_HS->Poll();
    m_HI->Poll();
    m_HO->Poll();

    int s_CS = m_CS->GetState();
    int s_HS = m_HS->GetState();
    int s_HI = m_HI->GetState();
    int s_HO = m_HO->GetState();

    if (s_CS == Valve::Fault || s_HS == Valve::Fault ||
        s_HI == Valve::Fault || s_HO == Valve::Fault)
    {
        m_state = Fault;
        m_Heater->Control(false);
    }
    else
    {
        switch (m_state)
        {
        case Closing:
            if (s_CS == Valve::Closed && s_HS == Valve::Closed &&
                s_HI == Valve::Closed && s_HO == Valve::Closed) {
                m_state = Closed;
            }
            break;

        case SwitchToCentral:
            if (s_HI == Valve::Closed && s_HO == Valve::Closed) {
                if (m_step == 0) {
                    m_CS->SetState(Valve::Open);
                    m_HS->SetState(Valve::Open);
                    s_CS = m_CS->GetState();
                    s_HS = m_HS->GetState();
                    m_step++;
                }
                if (m_step == 1) {
                    if (s_CS == Valve::Open && s_HS == Valve::Open) {
                        m_state = Central;
                    }
                }
            }
            break;

        case SwitchToHeater:
            if (s_HS == Valve::Closed) {
                if (m_step == 0) {
                    m_HI->SetState(Valve::Open);
                    m_HO->SetState(Valve::Open);
                    m_CS->SetState(Valve::Open);
                    s_HI = m_HI->GetState();
                    s_HO = m_HO->GetState();
                    s_HS = m_HS->GetState();
                    m_step++;
                }
                if (m_step == 1) {
                    if (s_HI == Valve::Open && s_HO == Valve::Open &&
                        s_CS == Valve::Open) {
                        m_Heater->Control(true);
                        m_state = Heater;
                    }
                }
            }
            break;

        default:
            break;
        }

        // Fold state of two valves into one for signalling wash enable
        // to the heater state machine:
        // Both Open => Open
        // One or both Opening; another can be Open => Opening
        // Anything else => Fault (will abort washing)
        int heaterInState;

        if ((s_CS == Valve::Open) && (s_HI == Valve::Open)) {
            heaterInState = Valve::Open;
        } else if (((s_CS == Valve::Open) || (s_CS == Valve::Opening)) &&
                   ((s_HI == Valve::Open) || (s_HI == Valve::Opening))) {
            heaterInState = Valve::Opening;
        } else {
            heaterInState = Valve::Fault;
        }

        m_Heater->Poll(heaterInState);
    }

    pthread_mutex_unlock(&m_Lock);
}

void HWState::ApplyState(state_t state)
{
    switch (state)
    {
    case Closed:
        m_Heater->Control(false);

        // This can be emergency, force-close
        m_CS->SetState(Valve::Closed, true);
        m_HS->SetState(Valve::Closed, true);
        m_HI->SetState(Valve::Closed, true);
        m_HO->SetState(Valve::Closed, true);
        m_state = Closing;
        break;

    case Central:
        m_Heater->Control(false);

        m_HI->SetState(Valve::Closed);
        m_HO->SetState(Valve::Closed);
        m_state = SwitchToCentral;
        break;

    case Heater:
        m_HS->SetState(Valve::Closed);
        m_state = SwitchToHeater;
        break;
    }

    m_step = 0;
}

void HWState::HeaterWash(bool on)
{
    if (on) {
        m_CS->SetState(Valve::Open);
        m_HI->SetState(Valve::Open);
    } else {
        ApplyState(m_state);
    }
}

bool HWState::InFinalState()
{
    return (m_state == Closed) || (m_state == Central) ||
           (m_state == Heater) || (m_state == Maintenance);
}

HWState::state_t HWState::GetState()
{
    return m_state;
}

int HWState::SetState(state_t state)
{
    int ret;
    const char* reason;

    if (state != Closed && state != Central && state != Heater) {
        // Errorneous input
        return EINVAL;
    }

    pthread_mutex_lock(&m_Lock);

    if (m_LeakSensor->GetState() == LeakSensor::Alarm) {
        ret = EPERM;
        reason = "leak detected";
    } else if (m_mode != Auto) {
        ret = 0;
        ApplyState(state);
    } else {
        ret = EPERM;
        reason = "not in manual mode";
    }

    pthread_mutex_unlock(&m_Lock);

    if (ret == EPERM) {
        Log(Log::ERROR) << "Manual system control denied: " << reason;
    }

    return ret;
}

void HWState::SetMode(ctlmode_t mode)
{
    Log(Log::INFO) << "Requested control mode: " << modeStrings[mode];

    pthread_mutex_lock(&m_Lock);

    m_mode = mode;

    pthread_mutex_unlock(&m_Lock);
}

int HWState::SetLeakState(LeakSensor::status_t state)
{
    if (state != LeakSensor::Enabled && state != LeakSensor::Disabled)
        return EINVAL;

    pthread_mutex_lock(&m_Lock);
    m_LeakSensor->SetState(state);
    pthread_mutex_unlock(&m_Lock);

    Log(Log::INFO) << "Leak sensor " << (state == LeakSensor::Enabled ?
                                         "enabled" : "disabled");

    return 0;
}

int HWState::SetHeaterState(int state)
{
    int ret;
    const char* reason;

    if (state != HeaterController::Wash)
        return EINVAL;

    Log(Log::INFO) << "Manual heater wash request";

    pthread_mutex_lock(&m_Lock);

    if (m_LeakSensor->GetState() == LeakSensor::Alarm) {
        ret = EPERM;
        reason = "leak detected";
    } else {
        m_Heater->SetState(state);
        ret = 0;
    }

    pthread_mutex_unlock(&m_Lock);

    if (ret == EPERM) {
        Log(Log::ERROR) << "Manual heater control denied: " << reason;
    }
}

int HWState::ValveControl(const char* id, int& state)
{
    Valve* hw = m_Cfg->GetHardware<Valve>(id);
    int reqState = state;
    int ret;

    if (!hw) {
        Log(Log::ERROR) << "Valve " << id << " not found";
        return ENOENT;
    }

    pthread_mutex_lock(&m_Lock);

    if (m_mode != FullManual) {
        ret = EPERM;
    } else if (hw->SetState(state, true)) {
        m_state = Maintenance;
        state = hw->GetState();
        ret = 0;
    } else {
        // Invalid input
        ret = EINVAL;
    }

    pthread_mutex_unlock(&m_Lock);

    switch (ret) {
    case 0:
        Log(Log::INFO) << hw->m_description << " manual " << Valve::statusStrings[reqState];
        break;
    case EPERM:
        Log(Log::ERROR) << hw->m_description << " manual " << Valve::statusStrings[reqState]
                        << " denied: not in maintenance mode";
        break;
    }

    return ret;
}

int HWState::RelayControl(const char* id, bool &state)
{
    Relay* hw = m_Cfg->GetHardware<Relay>(id);
    bool reqState = state;
    int ret;

    if (!hw) {
        Log(Log::ERROR) << "Relay " << id << " not found";
        return ENOENT;
    }

    pthread_mutex_lock(&m_Lock);

    if (m_mode != FullManual) {
        ret = EPERM;
    } else {
        hw->SetState(state);
        m_state = Maintenance;
        state = hw->GetState();
        ret = 0;
    }

    pthread_mutex_unlock(&m_Lock);

    switch (ret) {
    case 0:
        Log(Log::INFO) << hw->m_description << " manual " << Relay::statusStrings[reqState];
        break;
    case EPERM:
        Log(Log::ERROR) << hw->m_description << " manual " << Relay::statusStrings[reqState]
                       << " denied: not in maintenance mode";
        break;
    }

    return ret;
}
