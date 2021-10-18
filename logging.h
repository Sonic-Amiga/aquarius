#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <sstream>

extern unsigned int logMask;

class Log
{
public:
    typedef enum
    {
        ERR,
        WARN,
        INFO,
        DEBUG
    } Level;

    Log(Level kind) : m_Level(kind)
    {}
    ~Log();

    template <typename T>
    std::stringstream& operator<<(T val)
    {
        m_Stream << val;
        return m_Stream;
    }

private:
    Level m_Level;
    std::stringstream m_Stream;
};

void fatal(const char *fmt, ...);

class LogListener
{
public:
    virtual ~LogListener()
    {}

    void Write(Log::Level level, const std::string& line)
    {
        if (level <= m_Level) {
            Write(line);
        }
    }

protected:
    LogListener(Log::Level level = Log::Level::INFO) : m_Level(level) {}

private:
    virtual void Write(const std::string& line) = 0;

    Log::Level m_Level;
};

void AddLogListener(LogListener *);
void RemoveLogListener(LogListener *);

class FileLog : public LogListener
{
public:
    FileLog(Log::Level level, const char* path) : LogListener(level), m_Path(path)
    {}

private:
    virtual void Write(const std::string& line) override;

    std::string m_Path;
};

class ConsoleLog : public LogListener
{
public:
    ConsoleLog(Log::Level level) : LogListener(level) {}

private:
    virtual void Write(const std::string& line) override
    {
        std::cout << line << std::endl;
    }
};

#endif
