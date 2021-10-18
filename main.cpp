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

int main(void)
{
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

    return 0;
}
