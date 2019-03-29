#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>

#include "httpd.h"
#include "logging.h"
#include "userdb.h"
#include "utils.h"

void fatal(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(255);
}

class FileLog : public LogListener
{
public:
    FileLog(const char* path) : m_Path(path)
    {}

private:
    virtual void Write(const std::string& line) override;

    const char* m_Path;
};

void FileLog::Write(const std::string& line)
{
    std::fstream f(m_Path, std::fstream::out|std::fstream::app);

    f << line << std::endl;
    f.close();
}

class ConsoleLog : public LogListener
{
private:
	virtual void Write(const std::string& line) override
	{
		std::cout << line << std::endl;
	}
};

int main(void)
{
#ifdef _WIN32
    ConsoleLog mainLog;
#else
    FileLog mainLog("/var/log/aquarius.log");
#endif
    AddLogListener(&mainLog);
    InitUserDB();

    HWConfig* theConfig = new HWConfig();
    HTTPServer *theServer = new HTTPServer(theConfig, theConfig->m_HWState);

    Log(Log::INFO) << "System started";

    for (;;) {
        CheckSessions();
        theConfig->m_HWState->Poll();
        sleep(1);
    }

    delete theServer;
    delete theConfig;
    RemoveLogListener(&mainLog);

    return 0;
}
