#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <map>
#include <shared_mutex>
#include <string>
#include <variant>

typedef std::variant<int, float> GValue;

class EventBus
{
public:
    template<typename T>
    std::map<std::string, T> CollectValues(const std::string& prefix) const
    {
        size_t l = prefix.size();
        std::map<std::string, T> result;
        std::shared_lock readLock(g_mutex);

        for (const auto& it : g_values) {
            if (std::holds_alternative<T>(it.second) && it.first.compare(0, l, prefix) == 0) {
                result[it.first] = std::get<T>(it.second);
            }
        }

        return result;
    }

    std::map<std::string, GValue> CollectValues(const std::string& prefix) const
    {
        size_t l = prefix.size();
        std::map<std::string, GValue> result;
        std::shared_lock readLock(g_mutex);

        for (const auto& it : g_values) {
            if (it.first.compare(0, l, prefix) == 0) {
                result[it.first] = it.second;
            }
        }

        return result;
    }


    template<typename T>
    T getValue(const std::string& name, T def) const
    {
        std::shared_lock readLock(g_mutex);
        auto it = g_values.find(name);
        return it == g_values.end() ? def : std::get<T>(it->second);
    }

    static EventBus& getInstance() {return g_Bus; }

    void SendEvent(const std::string& topic, GValue value);

private:
    EventBus() = default;

    bool Update(const std::string& topic, GValue value)
    {
        std::unique_lock readLock(g_mutex);
        auto it = g_values.find(topic);

        if (it != g_values.end() && it->second == value) {
            return false;
        }

        g_values[topic] = value;
        return true;
    }

    std::map<std::string, GValue> g_values;
    mutable std::shared_mutex g_mutex;

    static EventBus g_Bus;
};

template <typename T>
void SendEvent(const std::string& topic, T value)
{
    EventBus::getInstance().SendEvent(topic, value);
}

#endif
