#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>

#include "httpd.h"
#include "logging.h"
#include "userdb.h"

void fatal(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
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

int main(void)
{
    FileLog fileLog("/var/log/aquarius.log");
    InitUserDB();

    HWConfig* theConfig = new HWConfig();
    HWState* theState = new HWState(theConfig);
    HTTPServer *theServer = new HTTPServer(theConfig, theState);

    Log(Log::INFO) << "System started";

    for (;;) {
        CheckSessions();
        theState->Poll();
        sleep(1);
    }

    delete theServer;
    delete theState;
    delete theConfig;

    return 0;
}
