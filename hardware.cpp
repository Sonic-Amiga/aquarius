#include <math.h>
#include <string>

#include "hardware.h"
#include "logging.h"
#include "utils.h"

const char* const Relay::statusStrings[] =
{
    "off",
    "on"
};

float Thermometer::GetValue()
{
    float temp = Measure();

    if (isnan(temp)) {
        m_State = Fault;
    } else if (temp >= m_Threshold) {
        m_State = Normal;
    } else {
        m_State = Cold;
    }

    return temp;
}

const char* const Valve::statusStrings[] =
{
    "reset",
    "close",
    "closing",
    "opening",
    "open",
    "fault"
};

Valve::Valve(time_t timeout,
             Relay* openPin, Relay* closePin,
             Switch* openSwitch, Switch* closeSwitch)
    : m_State(Reset),
      m_StateChange(0), m_StateChangeTimeout(timeout),
      m_OpenPin(openPin), m_ClosePin(closePin),
      m_OpenSwitch(openSwitch), m_CloseSwitch(closeSwitch)
{

}

Valve::~Valve()
{
    delete m_OpenPin;
    delete m_ClosePin;
    delete m_OpenSwitch;
    delete m_CloseSwitch;
}

void Valve::ReportFault(const char* s)
{
    // Avoid flooding the log
    if (m_State != Fault) {
        m_State = Fault;
        Log(Log::ERR) << m_description << " valve " << s;
    }
}

void Valve::GetStateFromSwitches()
{
    if (!HaveSwitches())
	return;

    int openSt  = m_OpenSwitch->GetState();
    int closeSt = m_CloseSwitch->GetState();

    if (openSt == Switch::Fault) {
        ReportFault("open sensor fault");
    } else if (closeSt == Switch::Fault) {
        ReportFault("close sensor fault");
    } else if ((openSt == Switch::On) && (closeSt == Switch::On)) {
        // This should not happen, likely electronics fault
        ReportFault("reports both open and closed");
    } else if ((openSt == Switch::On) && (m_State == Opening)) {
        ReportState(Open);
    } else if ((closeSt == Switch::On) && (m_State == Closing)) {
        ReportState(Closed);
    }
}

bool Valve::SetState(int state, bool force)
{
    bool change = false;

    if ((state != Open) && (state != Closed) && (state != Reset)) {
        // Errorneous input
        return false;
    }

    if ((m_State != Fault) || force) {
        switch (state)
        {
        case Open:
            m_ClosePin->SetState(false);
            m_OpenPin->SetState(true);
            if (m_State != Open) {
                if (m_State != Opening)
                    m_StateChange = GetMonotonicTime();
                ReportState(Opening);
            }
            break;

        case Closed:
            m_OpenPin->SetState(false);
            m_ClosePin->SetState(true);
            if (m_State != Closed) {
                if (m_State != Closing)
                    m_StateChange = GetMonotonicTime();
                ReportState(Closing);
            }
            break;

        case Reset:
            m_ClosePin->SetState(false);
            m_OpenPin->SetState(false);
            ReportState(Reset);
            break;
        }

        GetStateFromSwitches();
    }

    return true;
}

void Valve::Poll()
{
    GetStateFromSwitches();

    if ((m_State == Opening) || (m_State == Closing)) {
        if (GetMonotonicTime() - m_StateChange > m_StateChangeTimeout) {
	    if (HaveSwitches()) {
		// Timeout exceeded, mechanical fault
		Log(Log::ERR) << m_description << " valve "
		              << statusStrings[m_State] << " timeout";
		ReportState(Fault);
            } else if (m_State == Opening) {
		// Poor man's motors without position sensors. We just want for
		// enough time and assume they worked.
		ReportState(Open);
            } else {
		ReportState(Closed);
            }
        }
    }
}
