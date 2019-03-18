#include <microhttpd.h>

#include "hwconfig.h"
#include "hwstate.h"
#include "userdb.h"

class HTTPSession;

class HTTPServer
{
public:
    HTTPServer(HWConfig* cfg, HWState* hwState);
    ~HTTPServer();

    void Run();

private:
    int handleRequest(struct MHD_Connection *connection, const char *url);
    HTTPSession *findSession(struct MHD_Connection *connection);

    static int urlHandler(void *cls, struct MHD_Connection *connection, const char *url,
                          const char *method, const char *version,
                          const char *upload_data, size_t *upload_data_size, void **con_cls);
    static ssize_t readCallBack(void* cls, uint64_t pos, char *buf, size_t max);
    static void freeCallBack(void* cls);

    void formatFullStatus(std::ostream& output, HTTPSession* s);

    struct MHD_Daemon* m_httpd;
    HWConfig* m_hwConfig;
    HWState* m_hwState;
};
