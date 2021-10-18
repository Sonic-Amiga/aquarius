#include "event_bus.h"
#include "logging.h"

void SendEvent(const std::string& topic, int value)
{
    Log(Log::DEBUG) << topic << " = " << value;
}

void SendEvent(const std::string& topic, float value)
{
    Log(Log::DEBUG) << topic << " = " << value;
}
