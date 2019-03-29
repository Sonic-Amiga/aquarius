#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "logging.h"
#include "utils.h"

static std::vector<LogListener *> g_Listeners;
static std::mutex g_Lock;

static const char* logTags[] =
{
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG",
    NULL
};

Log::~Log()
{
    time_t t = time(nullptr);
    struct tm pt;
    char ts[128];

    localtime_r(&t, &pt);
    strftime(ts, sizeof(ts), "%d.%m.%Y %H:%M:%S", &pt);

    std::string text = std::string("[") + logTags[m_Level] + "] " + ts + ' ' + m_Stream.str();

    g_Lock.lock();

    for (LogListener* l : g_Listeners) {
        l->Write(text);
    }

    g_Lock.unlock();
}

LogListener::LogListener()
{
    g_Lock.lock();
    g_Listeners.push_back(this);
    g_Lock.unlock();
}

LogListener::~LogListener()
{
    g_Lock.lock();

    for (auto it = g_Listeners.begin(); it != g_Listeners.end(); it++) {
        if (*it == this) {
            g_Listeners.erase(it);
            break;
        }
    }

    g_Lock.unlock();
}
