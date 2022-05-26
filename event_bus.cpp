#include "event_bus.h"
#include "logging.h"

// This is our global singletone
EventBus EventBus::g_Bus;

std::ostream& operator<<(std::ostream& os, GValue v)
{
    if (std::holds_alternative<int>(v))
        os << "<int>" << std::get<int>(v);
    else if (std::holds_alternative<float>(v))
        os << "<float>" << std::get<float>(v);
    else
        os << "<none>";

    return os;
}

void EventBus::SendEvent(const std::string& topic, GValue value)
{
    if (!Update(topic, value)) {
        // Nothing has changed, don't bother listeners
        return;
    }

    Log(Log::DEBUG) << topic << " = " << value;
}

