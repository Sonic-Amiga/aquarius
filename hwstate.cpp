#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#define O_BINARY 0
#endif

#include "hwstate.h"
#include "logging.h"
#include "userdb.h"
#include "utils.h"
#include "wiringpi_hw.h"

LeakSensor::LeakSensor(HWConfig* cfg) : m_state(Enabled)
{
    m_Sensors = cfg->GetLeakDetectors();
    m_SensorState = new int[m_Sensors.size()];

    for (size_t i = 0; i < m_Sensors.size(); i++)
        m_SensorState[i] = Switch::Off;
}

bool LeakSensor::Poll()
{
    bool alarm = false;

    for (size_t i = 0; i < m_Sensors.size(); i++) {
        Switch* s = m_Sensors[i];
        int ss = s->poll();

        if (m_SensorState[i] == ss) {
            // Avoid flooding the log
            continue;
        }

        m_SensorState[i] = ss;

        switch (ss) {
        case Switch::On:
            Log(Log::WARN) << "Leak detected in " << s->m_description;
            if (m_state == Enabled) {
                ReportState(Alarm);
                alarm = true;
            }
            break;

        case Switch::Fault:
            Log(Log::ERR) << "Leak sensor fault in " << s->m_description;
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
    m_Heater      = cfg->GetHardware<Relay>("HR");
    m_Drain       = cfg->GetHardware<Relay>("HD");
    m_Pressure    = cfg->GetHardware<Switch>("HP");
    m_Temperature = cfg->GetHardware<Thermometer>("HT");

    // We know functions, so we know descriptions
    m_Heater->m_description      = "Heater relay";
    m_Drain->m_description       = "Heater drain";
    m_Pressure->m_description    = "Heater pressure";
    m_Temperature->m_description = "Heater temperature";

    m_Pressure->SetStatePrefix("pressure_switch");
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
    ReportState(state);

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
    int s_HP = m_Pressure->poll();

    // Currently we don't do anything with heater temperature, it's just
    // for the user, but someone has to poll it, do it here
    m_Temperature->GetValue();

    if (s_HP == Switch::Fault) {
        if (m_State != Fault) {
            Log(Log::ERR) << "Heater pressure monitor fault";
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
                m_washTimer = GetMonotonicTime() + WashDelay;
                m_washStep = WashStep::Drain;
            }
            if ((m_washStep == WashStep::Drain) && (GetMonotonicTime() > m_washTimer)) {
                RefillAndEndWash();
            }
        } else if ((s_HI != Valve::Opening) && (m_washStep != WashStep::Refill)) {
            Log(Log::WARN) << "Heater wash aborted";
            RefillAndEndWash();
        }

        // Refill is done even after the abort
        if (m_washStep == WashStep::Refill) {
            switch (s_HP)
            {
            case Switch::Off:
                if (GetMonotonicTime() > m_washTimer) {
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
        ReportState(Wash);
        m_washStep = Fill;
    }
}

void HeaterController::RefillAndEndWash()
{
    m_Drain->SetState(false);
    m_washTimer = GetMonotonicTime() + RefillDelay;
    m_washStep = WashStep::Refill;
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

static const char *stateStrings[] =
{
    "Fault",
    "Closing",
    "Closed",
    "Switch to cenral",
    "Central",
    "Switch to heater",
    "Heater",
    "Maintenance"
};

HWState::HWState(HWConfig* cfg, Valve *CS, Valve *HS, Valve *HI, Valve *HO, Thermometer *HST,
                 time_t recoverDelay)
    : m_Cfg(cfg), m_CS(CS), m_HS(HS), m_HI(HI), m_HO(HO), m_HST(HST),
      m_state(Maintenance), m_mode(Manual),
      // If hot water is OK at startup, we'll switch immediately.
      m_RecoverTime(~0), m_RecoverDelay(recoverDelay)
{
    // We know functions, so we know descriptions
    m_CS->m_description = "Cold supply";
    m_HS->m_description = "Hot supply";
    m_HI->m_description = "Heater input";
    m_HO->m_description = "Heater output";

    m_LeakSensor = new LeakSensor(cfg);
    m_Heater     = new HeaterController(this, cfg);

    if (!LoadState()) {
        Log(Log::ERR) << "Could not read saved status; fall back to default!";
    }
}

HWState::~HWState()
{
    delete m_LeakSensor;
    delete m_Heater;
}

#ifdef _WIN32
static const char *stateFile = "C:\\aquarius\\aquarius.state";
#else
// /var/run is tmpfs on OrangePI
static const char *stateFile = "/var/aquarius.state";
#endif

bool HWState::LoadState()
{
    struct SavedState st;
    int fd = open(stateFile, O_RDONLY|O_BINARY);

    // The file must exist and be readable
    if (fd == -1) {
        return false;
    }

    size_t s = read(fd, &st, sizeof(st));

    close(fd);

    // File length must be correct
    if (s != sizeof(st)) {
        return false;
    }
    // State field cannot contain garbage
    if (!IsFinalState(st.State)) {
        return false;
    }
    // Mode field cannot contain garbage
    if ((unsigned int)st.Mode > (unsigned int)FullManual) {
       return false;
    }
    // Check value must be correct
    if (st.CalcCheck() != st.Check) {
        return false;
    }

    ReportMode(st.Mode);
    if (m_mode == Manual) {
        // After we exit relays stay in their original states, but configuring
        // I/O hardware switches all of them off. Wait 0.5 sec before
        // turning back on some of them; quickly pulsing them isn't good for
        // electronics
        msleep(500);
        // In auto mode we'll deduce the state to set, and 
        // in Maintenance mode we only do what operator is telling
        Log(Log::INFO) << "Bringing back manual control state: "
                       << stateStrings[st.State];
        ApplyState(st.State);
    }

    return true;
}

bool HWState::SaveState(state_t state, ctlmode_t mode)
{
    struct SavedState st;
    bool ok;

    st.State = state;
    st.Mode  = mode;
    st.Check = st.CalcCheck();

    int fd = open(stateFile, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0600);

    if (fd == -1) {
        ok = false;
    } else {
        ok = true;

        // We want to be sure that write has completed successfully
        if (write(fd, &st, sizeof(st)) != sizeof(st))
            ok = false;
#ifdef __unix__
        if (fsync(fd))
            ok = false;
#endif
        if (close(fd))
            ok = false;
    }

    if (!ok) {
        Log(Log::ERR) << "Unable to save status file";
    }

    return ok;
}

void HWState::Poll()
{
    m_Lock.lock();

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
        ReportState(Fault);
        m_Heater->Control(false);
    }
    else
    {
        switch (m_state)
        {
        case Closing:
            if (s_CS == Valve::Closed && s_HS == Valve::Closed &&
                s_HI == Valve::Closed && s_HO == Valve::Closed) {
                ReportState(Closed);
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
                        ReportState(Central);
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
                        ReportState(Heater);
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

    m_HST->GetValue(); // This updates the thermometer state
    int hstState = m_HST->GetState();

    switch (hstState)
    {
    case Thermometer::Fault:
        // No idea what to do yet; we cannot make any decisions
        // The notification has already been sent by the thermometer class itself
        m_RecoverTime = 0;
        break;

    case Thermometer::Cold:
        m_RecoverTime = 0;
        if (AutoModeOK() &&
            ((m_state == Central) || (m_state == Closed) || (m_state == Maintenance)))
        {
            Log(Log::WARN) << "Hot water temperature dropped, switching to heater";
            ApplyState(Heater);
        }
        break;

    case Thermometer::Normal:
        if (m_RecoverTime == 0) {
            m_RecoverTime = GetMonotonicTime() + m_RecoverDelay;
        } else if ((GetMonotonicTime() >= m_RecoverTime) && AutoModeOK() &&
                   ((m_state == Heater) || (m_state == Closed) || (m_state == Maintenance)))
        {
            Log(Log::INFO) << "Hot water temperature restored, switching to central supply";
            ApplyState(Central);
        }
     
        break;
    }

    m_Lock.unlock();
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
        ReportState(Closing);
        break;

    case Central:
        m_Heater->Control(false);

        m_HI->SetState(Valve::Closed);
        m_HO->SetState(Valve::Closed);
        ReportState(SwitchToCentral);
        break;

    case Heater:
        m_HS->SetState(Valve::Closed);
        ReportState(SwitchToHeater);
        break;

    default:
        ReportState(state);
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

bool HWState::IsFinalState(state_t state)
{
    // Returns true for non-transient states
    return (state == Closed) || (state == Central) ||
           (state == Heater) || (state == Maintenance);
}

int HWState::SetState(state_t state, const std::string &user)
{
    int ret;
	const char *action;
    const char *reason;

	switch (state)
	{
	case Closed:
		action = "Manual close all";
		break;
	case Central:
		action = "Manual switch to central";
		break;
	case Heater:
		action = "Manual switch to heater";
		break;
	default:
		// Errorneous input
		return EINVAL;
	}

    m_Lock.lock();

    if (m_LeakSensor->GetState() == LeakSensor::Alarm) {
        ret = EPERM;
        reason = "leak detected";
    } else if (m_mode != Auto) {
        SaveState(state, m_mode);
        ret = 0;
        ApplyState(state);
    } else {
        ret = EPERM;
        reason = "not in manual mode";
    }

    m_Lock.unlock();

	switch (ret)
	{
	case 0:
		Log(Log::INFO) << user << ' ' << action;
		break;

	case EPERM:
        Log(Log::ERR) << user << " Manual system control denied: " << reason;
		break;
    }

    return ret;
}

void HWState::SetMode(ctlmode_t mode, const std::string &user)
{
    Log(Log::INFO) << user << " Requested control mode: " << modeStrings[mode];

    m_Lock.lock();

    SaveState(m_state, mode);
    ReportMode(mode);

    m_Lock.unlock();
}

int HWState::SetLeakState(LeakSensor::status_t state, const std::string &user)
{
    if (state != LeakSensor::Enabled && state != LeakSensor::Disabled)
        return EINVAL;

    m_Lock.lock();
    m_LeakSensor->SetState(state);
    m_Lock.unlock();

    Log(Log::INFO) << user << " Leak sensor "
		           << (state == LeakSensor::Enabled ? "enabled" : "disabled");

    return 0;
}

int HWState::SetHeaterState(int state, const std::string &user)
{
    int ret;

    if (state != HeaterController::Wash)
        return EINVAL;

    m_Lock.lock();

    if (m_LeakSensor->GetState() == LeakSensor::Alarm) {
        ret = EPERM;
    } else {
        m_Heater->SetState(state);
        ret = 0;
    }

    m_Lock.unlock();
 
	switch (ret)
	{
	case 0:
		Log(Log::INFO) << user << " Manual heater wash request";
		break;

	case EPERM:
        Log(Log::ERR) << user << " Manual heater control denied: leak detected";
		break;
    }

	return ret;
}

int HWState::ValveControl(const char* id, int& state, const std::string& user)
{
    Valve* hw = m_Cfg->GetHardware<Valve>(id);
    int reqState = state;
    int ret;

    if (!hw) {
        Log(Log::ERR) << user << " Valve " << id << " not found";
        return ENOENT;
    }

    m_Lock.lock();

    if (m_mode != FullManual) {
        ret = EPERM;
    } else if (hw->SetState(state, true)) {
        ReportState(Maintenance);
        state = hw->GetState();
        ret = 0;
    } else {
        // Invalid input
        ret = EINVAL;
    }

    m_Lock.unlock();

    switch (ret) {
    case 0:
        Log(Log::INFO) << user << ' ' << hw->m_description << " manual "
			           << Valve::statusStrings[reqState];
        break;
    case EPERM:
        Log(Log::ERR) << user << ' ' << hw->m_description << " manual "
			          << Valve::statusStrings[reqState] << " denied: not in maintenance mode";
        break;
    }

    return ret;
}

int HWState::RelayControl(const char* id, bool &state, const std::string& user)
{
    Relay* hw = m_Cfg->GetHardware<Relay>(id);
    bool reqState = state;
    int ret;

    if (!hw) {
        Log(Log::ERR) << user << " Relay " << id << " not found";
        return ENOENT;
    }

    m_Lock.lock();

    if (m_mode != FullManual) {
        ret = EPERM;
    } else {
        hw->SetState(state);
        ReportState(Maintenance);
        state = hw->GetState();
        ret = 0;
    }

    m_Lock.unlock();

    switch (ret) {
    case 0:
        Log(Log::INFO) << user << ' ' << hw->m_description << " manual "
			           << Relay::statusStrings[reqState];
        break;
    case EPERM:
        Log(Log::ERR) << user << ' ' << hw->m_description << " manual "
			          << Relay::statusStrings[reqState] << " denied: not in maintenance mode";
        break;
    }

    return ret;
}
