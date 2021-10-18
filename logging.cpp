#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "hwconfig.h"
#include "logging.h"
#include "utils.h"

static std::vector<LogListener *> g_Listeners;
static std::mutex g_Lock;

void AddLogListener(LogListener *l)
{
	g_Lock.lock();
	g_Listeners.push_back(l);
	g_Lock.unlock();
}

void RemoveLogListener(LogListener *l)
{
	g_Lock.lock();

	for (auto it = g_Listeners.begin(); it != g_Listeners.end(); it++) {
		if (*it == l) {
			g_Listeners.erase(it);
			break;
		}
	}

	g_Lock.unlock();
}

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
        l->Write(m_Level, text);
    }

    g_Lock.unlock();
}

static Log::Level getLogLevel(xmlNode* node)
{
    const char *level = GetStrProp(node, "level");

    if (level) {
	for (int i = 0; logTags[i]; i++) {
	    if (!strcmp(level, logTags[i])) {
		return (Log::Level)i;
	    }
	}

	std::cerr << "Invalid log level " << level << std::endl;
    }

    return Log::Level::INFO;
}

REGISTER_LOGGER_TYPE(console)(xmlNode* node, HWConfig*)
{
    return new ConsoleLog(getLogLevel(node));
}

REGISTER_LOGGER_TYPE(file)(xmlNode* node, HWConfig*)
{
    const char* path = GetStrProp(node, "path");

    return new FileLog(getLogLevel(node), path);
}

void FileLog::Write(const std::string& line)
{
    std::fstream f(m_Path, std::fstream::out | std::fstream::app);

    f << line << std::endl;
    f.close();
}
