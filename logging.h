#ifndef LOGGING_H
#define LOGGING_H

#include <sstream>

extern unsigned int logMask;

class Log
{
public:
    typedef enum
    {
        ERROR,
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
    LogListener();
    virtual ~LogListener();

    virtual void Write(const std::string& line) = 0;
};

#endif
