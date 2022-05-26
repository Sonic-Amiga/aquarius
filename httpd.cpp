#include <fcntl.h>
#include <math.h>
#include <string.h>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include "event_bus.h"
#include "httpd.h"
#include "logging.h"
#include "userdb.h"

// TODO: These have to go to some config file
static const unsigned short port = 80;
#ifdef _WIN32
static const char* webRoot = "C:\\aquarius\\share\\aquarius\\web";
#else
static const char* webRoot = "/usr/local/share/aquarius/web";
#endif

class FileReader : public std::fstream
{
public:
    FileReader(const char *path, std::vector<std::pair<std::string, std::string>>& kv);
    ~FileReader();

    int Read(char*s, int size);

private:
    std::fstream f;
    std::string buf;
    size_t pos;

    std::vector<std::pair<std::string, std::string>> keyValues;
};

FileReader::FileReader(const char *localPath, std::vector<std::pair<std::string, std::string>>& kv)
    :pos(0), keyValues(kv)
{
    std::string path = std::string(webRoot) + localPath;

    open(path.c_str(), std::fstream::in);
}

int FileReader::Read(char *s, int size)
{
    if (pos >= buf.length()) {
        int p;

        if (eof()) {
            return MHD_CONTENT_READER_END_OF_STREAM;
        } else if (!good()) {
            return MHD_CONTENT_READER_END_WITH_ERROR;
        }

        std::getline(*this, buf);
        // Put the EOL back
        buf += '\n';
        pos = 0;


        p = 0;
        while ((p = buf.find('%', p)) != std::string::npos) {
            p++;
            for (auto &kv : keyValues) {
                int l = kv.first.length();

                if ((!buf.compare(p, l, kv.first)) && (buf[p + l] == '%')) {
                    p--;
                    buf.replace(p, l + 2, kv.second);
                    p += kv.second.length();
                    break;
                }
            }
        }
    }

    int remaining = buf.length() - pos;

    if (size > remaining) {
        size = remaining;
    }

    buf.copy(s, size, pos);
    pos += size;

    return size;
}

FileReader::~FileReader()
{
    f.close();
}

class HTTPSession : public Session, public LogListener
{
public:
    HTTPSession(const char* user, const std::string& connId, unsigned int access)
		: Session(user, connId), m_Access(access)
    {
		AddLogListener(this);
	}

	virtual ~HTTPSession()
	{
		RemoveLogListener(this);
	}

    std::vector<std::string> Read()
    {
        m_Lock.lock();
        std::vector<std::string> lines = m_Lines;
        m_Lines.clear();
        m_Lock.unlock();

        return lines;
    }

	unsigned int m_Access;

private:
    virtual void Write(const std::string& line) override
    {
        m_Lock.lock();
        m_Lines.push_back(line);
        m_Lock.unlock();
    }

    std::vector<std::string> m_Lines;
    std::mutex m_Lock;
};

int HTTPServer::urlHandler(void *cls, struct MHD_Connection *connection, const char *url,
                           const char *method, const char *version,
                           const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    HTTPServer* pServer = (HTTPServer*)cls;

    return pServer->handleRequest(connection, url);
}

ssize_t HTTPServer::readCallBack(void* cls, uint64_t pos, char *buf, size_t max)
{
    FileReader* r = (FileReader *)cls;

    return r->Read(buf, max);
}

void HTTPServer::freeCallBack(void* cls)
{
    FileReader* r = (FileReader *)cls;

    delete r;
}

HTTPServer::HTTPServer(HWConfig* cfg, HWState* hwState)
    : m_hwConfig(cfg), m_hwState(hwState)
{
    m_httpd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY|MHD_USE_DEBUG, port,
                               NULL, NULL, urlHandler, this,
                               MHD_OPTION_END);
    if (!m_httpd) {
        fatal("Failed to create httpd");
    }
}

HTTPServer::~HTTPServer()
{
    MHD_stop_daemon(m_httpd);
}

void HTTPServer::Run()
{
    if (MHD_run(m_httpd) != MHD_YES) {
        fatal("Failed to start httpd");
    }
}

static void formatStates(std::ostream& output, const char* prefix)
{
    size_t l = strlen(prefix) + 1;
    std::map<std::string, int> states = EventBus::getInstance().CollectValues<int>(prefix);
    bool first = true;

    output << '{';

    for (const auto& it : states) {
        size_t end = it.first.find('/', l);
        if (end != std::string::npos) {
            if (!first)
                output << ',';
            first = false;
            output << '"' << it.first.substr(l, end - l) << "\":" << it.second;
        }
    }

    output << '}';
}

struct StateValue
{
    int state    = -1;
    float value = NAN;
};

static void formatValues(std::ostream& output, const char* prefix)
{
    size_t l = strlen(prefix) + 1;
    std::map<std::string, GValue> states = EventBus::getInstance().CollectValues(prefix);
    std::map<std::string, StateValue> grouped;
    bool first = true;

    // Values are given in a nondeterministic order, group them per device
    for (const auto& it : states) {
        size_t end = it.first.find('/', l);
        if (end == std::string::npos)
            continue;

        std::string name = it.first.substr(l, end - l);

        end++; // 'end' points at '/', move past

        if (!it.first.compare(end, std::string::npos, "state")) {
            grouped[name].state = std::get<int>(it.second);
        } else if (!it.first.compare(end, std::string::npos, "value")) {
            grouped[name].value = std::get<float>(it.second);
        }
    }

    output << '{';
    for (const auto& it : grouped) {
        if (!first)
            output << ',';
        first = false;
        output << '"' << it.first << "\":{";
        if (!isnan(it.second.value)) {
            output << "\"value\":" << it.second.value;
            if (it.second.state != -1)
                output << ',';
        }
        if (it.second.state != -1) {
            output << "\"state\":" << it.second.state;
        }
        output << '}';
    }
    output << '}';
}

static void formatSingleValue(std::ostream& os, const char* json_name, const char* name)
{
    int state = EventBus::getInstance().getValue<int>(name, -1);

    if (state == -1)
        return;

    os << ",\"" << json_name << "\":" << state;
}

static void formatLog(std::ostream& output, HTTPSession *s)
{
    std::vector<std::string> log = s->Read();
    int size = log.size();

    if (size) {
        output << ",\"log\":[";
        for (int i = 0; i < size; i++) {
            output << '"' << log[i] << '"';
            if (i < size - 1)
                output << ',';
        }
        output << ']';
    }
}

static void formatItemStatus(std::ostream& output, const char* id, int state)
{
    output << "{\"" << id << "\":" << state << '}';
}

void HTTPServer::formatFullStatus(std::ostream& output, HTTPSession* s)
{
    output << "{\"valves\":";
    formatStates(output, "valve");
    output << ",\"relays\":";
    formatStates(output, "relay");
    output << ",\"switches\":";
    formatStates(output, "pressure_switch");
    output << ",\"thermometers\":";
    formatValues(output, "thermometer");
    output << ",\"leak_sensors\":";
    formatStates(output, "leak_sensor");
    formatSingleValue(output, "sys", "ValveController/state");
    formatSingleValue(output, "mode", "ValveController/mode");
    formatSingleValue(output, "leak", "LeakDetector/state");
    formatSingleValue(output, "heater", "HeaterController/state");

    output << ",\"sys\":" << m_hwState->GetState();
    output << ",\"mode\":" << m_hwState->GetMode();
    output << ",\"leak\":" << m_hwState->GetLeakState();
    output << ",\"heater\":" << m_hwState->GetHeaterState();
    formatLog(output, s);
    output << '}';
}

static std::string getConnId(struct MHD_Connection *conn)
{
    struct sockaddr *sa = MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
    const void *addr = nullptr;
    char buf[64];

    switch (sa->sa_family)
    {
    case AF_INET:
        addr = &((struct sockaddr_in *)sa)->sin_addr;
        break;
    case AF_INET6:
        addr = &((struct sockaddr_in6 *)sa)->sin6_addr;
        break;
    default:
        // Should never get here
        return std::string("unknown") + std::to_string(sa->sa_family) + "/http";
    }

    inet_ntop(sa->sa_family, addr, buf, sizeof(buf));
    return std::string(buf) + "/http";
}

HTTPSession* HTTPServer::findSession(struct MHD_Connection *connection, unsigned int permission)
{
    const char* sidStr = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "session");

    if (sidStr) {
        long sid = strtoul(sidStr, nullptr, 10);
        HTTPSession *s = dynamic_cast<HTTPSession *>(GetSession(sid));

        if (s && (s->m_ConnId == getConnId(connection))
			  && (s->m_Access >= permission))
		{
            return s;
	    }
    }

    return nullptr;
}

unsigned int HTTPServer::GetControlUserLevel()
{
	// If the system in maintenance mode, only technician can control
	return m_hwState->GetMode() == HWState::FullManual ? User::TECHNICIAN : User::NORMAL;
}

int HTTPServer::handleRequest(struct MHD_Connection *connection, const char* url)
{
    struct MHD_Response *response;
    int res = MHD_HTTP_BAD_REQUEST;
    int ret;
    std::stringstream output;
    const char* localPath = nullptr;
    std::string redirect;
    FileReader* reader = nullptr;
    HTTPSession* s = nullptr;

    if (!strcmp(url, "/auth")) {
        const char *user   = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "user");
        const char *passwd = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "password");

        if (user && passwd) {
            unsigned int permissions = Authenticate(user, passwd);

            if (permissions > User::NOACCESS) {
                s = new HTTPSession(user, getConnId(connection), permissions);
                RegisterSession(s);
            }
        }

        if (s) {
            redirect = std::string("/panel.html?session=") + std::to_string(s->m_Id);
        } else {
            redirect = "/noaccess.html";
        }
    } else if (!strcmp(url, "/logout")) {
        s = findSession(connection);

        if (s) {
            TerminateSession(s);
            s = nullptr;
        }

        redirect = "/index.html";
    } else if (!strcmp(url, "/valve")) {
        s = findSession(connection, User::TECHNICIAN);
        if (s) {
            const char *id = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "id");
            const char *action = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "action");
            int state = Valve::Fault;

            if (id && action) {
                if (!strcmp(action, "close"))
                    state = Valve::Closed;
                else if (!strcmp(action, "open"))
                    state = Valve::Open;
                else if (!strcmp(action, "reset"))
                    state = Valve::Reset;

                int err = m_hwState->ValveControl(id, state, s->GetConnStr());

                if (err == 0) {
                    output << "{\"valves\":";
                    formatItemStatus(output, id, state);
                    formatLog(output, s);
                    output << '}';
                    res = MHD_HTTP_OK;
                }
            }
        } else {
            res = MHD_HTTP_UNAUTHORIZED;
        }
    } else if (!strcmp(url, "/relay")) {
        s = findSession(connection, User::TECHNICIAN);
        if (s) {
            const char *id = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "id");
            const char *action = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "action");

            if (id && action) {
                int err = EINVAL;
                bool state;

                if (!strcmp(action, "off")) {
                    state = false;
                    err = 0;
                } else if (!strcmp(action, "on")) {
                    state = true;
                    err = 0;
                }

                if (err == 0) {
                    err = m_hwState->RelayControl(id, state, s->GetConnStr());
                }

                if (err == 0) {
                    output << "{\"relays\":";
                    formatItemStatus(output, id, state);
                    formatLog(output, s);
                    output << '}';
                    res = MHD_HTTP_OK;
                }
            }
        } else {
            res = MHD_HTTP_UNAUTHORIZED;
        }
    } else if (!strcmp(url, "/status")) {
        s = findSession(connection);
        if (s) {
            formatFullStatus(output, s);
            res = MHD_HTTP_OK;
        } else {
            res = MHD_HTTP_UNAUTHORIZED;
        }
    } else if (!strcmp(url, "/control")) {
        s = findSession(connection, GetControlUserLevel());
        if (s) {
            if (const char *modeStr = MHD_lookup_connection_value(connection,
                                                                  MHD_GET_ARGUMENT_KIND, "mode"))
            {
                HWState::ctlmode_t mode = HWState::BadMode;

                if (!strcmp(modeStr, "auto")) {
                    mode = HWState::Auto;
                } else if (!strcmp(modeStr, "manual")) {
                    mode = HWState::Manual;
                } else if (!strcmp(modeStr, "maintenance")) {
					// Only technician can switch to maintenance mode
					if (s->m_Access >= User::TECHNICIAN)
						mode = HWState::FullManual;
					else
						res = MHD_HTTP_UNAUTHORIZED;
                }

                if (mode != HWState::BadMode) {
                    m_hwState->SetMode(mode, s->GetConnStr());
                    formatFullStatus(output, s);
                    res = MHD_HTTP_OK;
                }
            }
            else if (const char *stateStr = MHD_lookup_connection_value(connection,
                                                                        MHD_GET_ARGUMENT_KIND, "state"))
            {
                HWState::state_t state = HWState::Fault;

                if (!strcmp(stateStr, "closed")) {
                    state = HWState::Closed;
                } else if (!strcmp(stateStr, "central")) {
                    state = HWState::Central;
                } else if (!strcmp(stateStr, "heater")) {
                    state = HWState::Heater;
                }
                if (state != HWState::Fault) {
                    m_hwState->SetState(state, s->GetConnStr());
                    formatFullStatus(output, s);
                    res = MHD_HTTP_OK;
                }
            }
            else if (const char* leakStr = MHD_lookup_connection_value(connection,
                                                                   MHD_GET_ARGUMENT_KIND, "leak"))
            {
                LeakSensor::status_t state = LeakSensor::Fault;

                if (!strcmp(leakStr, "enable")) {
                    state = LeakSensor::Enabled;
                } else if (!strcmp(leakStr, "disable")) {
                    state = LeakSensor::Disabled;
                }
                if (state != LeakSensor::Fault) {
                    m_hwState->SetLeakState(state, s->GetConnStr());
                    formatFullStatus(output, s);
                    res = MHD_HTTP_OK;
                }
            }
            else if (const char* heaterStr = MHD_lookup_connection_value(connection,
                                                                   MHD_GET_ARGUMENT_KIND, "heater"))
            {
                int state = HeaterController::Fault;

                if (!strcmp(heaterStr, "wash")) {
                    state = HeaterController::Wash;
                }
                if (state != HeaterController::Fault) {
                    m_hwState->SetHeaterState(state, s->GetConnStr());
                    formatFullStatus(output, s);
                    res = MHD_HTTP_OK;
                }
            }
        } else {
            res = MHD_HTTP_UNAUTHORIZED;
        }
    } else if (!(strcmp(url, "/") && strcmp(url, "/index.htm"))) {
        localPath = "/index.html";
    } else {
        localPath = url;
    }

    if (!strcmp(url, "/panel.html")) {
        // TODO: Define protected zone in some different, flexible way
        s = findSession(connection);
        if (!s) {
            localPath = nullptr;
            redirect = "/index.html";
        }
    }

    if (localPath) {
        std::vector<std::pair<std::string, std::string>> keyValues;

        if (s) {
            keyValues.push_back(std::make_pair("SESSIONID", std::to_string(s->m_Id)));
        }

        reader = new FileReader(localPath, keyValues);

        if (!reader->good()) {
            delete reader;
            reader = nullptr;
            res = MHD_HTTP_NOT_FOUND;
        }

    }

    if (reader) {
        response = MHD_create_response_from_callback(-1, 256, readCallBack, reader, freeCallBack);
    } else if (!redirect.empty()) {
        static const char* redirText = "Sorry, your browser is not supported";

        response = MHD_create_response_from_buffer(strlen(redirText), (void *)redirText, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, MHD_HTTP_HEADER_LOCATION, redirect.c_str());
        res = MHD_HTTP_TEMPORARY_REDIRECT;
    } else {
        std::string str;
        const char* errStr;

        switch (res)
        {
        case MHD_HTTP_OK:
            errStr = nullptr;
            break;

        case MHD_HTTP_BAD_REQUEST:
            errStr = "Bad request";

        case MHD_HTTP_UNAUTHORIZED:
            errStr = "Unauthorized";
            break;

        case MHD_HTTP_NOT_FOUND:
            errStr = "Not found";
            break;

        default:
            errStr = "Unknown error";
            break;
        }

        if (errStr) {
            str = std::string("<html><body>") + std::to_string(res) +
                  ' ' + errStr + "</body></html>";
        } else {
            str = output.str();
        }

        response = MHD_create_response_from_buffer(str.length(), (void*)str.c_str(), MHD_RESPMEM_MUST_COPY);
    }

    ret = MHD_queue_response(connection, res, response);
    MHD_destroy_response(response);

    return ret;
}
