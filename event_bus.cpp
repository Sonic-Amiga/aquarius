#include <map>
#include <variant>

#include "event_bus.h"
#include "logging.h"

typedef std::variant<int, float> GValue;

std::ostream& operator<<(std::ostream& os, GValue v)
{
    if (std::holds_alternative<int>(v))
        os << "<int>" << std::get<int>(v);
    else if (std::holds_alternative<float>(v))
        os << "<float>" << std::get<float>(v);
    else
        os << "<UNKNOWN TYPE>";

    return os;
}

std::map<std::string, GValue> g_values;

void SendEvent(const std::string& topic, GValue value)
{
    auto it = g_values.find(topic);

    if (it != g_values.end() && it->second == value) {
        // Nothing has changed, don't bother listeners
        return;
    }

    g_values[topic] = value;
    Log(Log::DEBUG) << topic << " = " << value;
}

void SendEvent(const std::string& topic, int value)
{
    SendEvent(topic, GValue(value));
}

void SendEvent(const std::string& topic, float value)
{
    SendEvent(topic, GValue(value));
}
