#include <string.h>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>

#include "logging.h"
#include "userdb.h"
#include "utils.h"

static std::map<unsigned long, Session*> g_Sessions;
static std::mutex g_Lock;

static const unsigned int ExpireTime = 30;

static std::map<std::string, User> g_UserDB;

static std::string getWord(const std::string& str, int& pos)
{
    if (pos == str.length()) {
        return std::string();
    }

    while (isspace(str[pos])) {
        pos++;
        if (pos == str.length()) {
            return std::string();
        }
    }

    int start = pos;

    while (!isspace(str[pos])) {
        pos++;
        if (pos == str.length()) {
            break;
        }
    }

    return str.substr(start, pos - start);
}

static int getInt(const std::string& str, int& pos)
{
    if (pos == str.length()) {
        return -1;
    }

    while (isspace(str[pos])) {
        pos++;
        if (pos == str.length()) {
            return -1;
        }
    }

    const char *s = str.c_str() + pos;
    char *end;
    int val = strtoul(s, &end, 10);
    pos += end - s;

    return val;
}

// TODO: Shouldn't this be configurable ?
#ifdef _WIN32
static const char *const users_path = "C:\\aquarius\\etc\\aquarius\\users";
#else
static const char *const users_path = "/etc/aquarius/users";
#endif // _WIN32

void InitUserDB()
{
    std::fstream f(users_path, std::fstream::in);
    std::string user, passwd;
    int access;

    if (!f.good()) {
        fatal("Unable to read user database %s", users_path);
    }

    while (!f.eof()) {
        std::string line;
        int pos = 0;

        std::getline(f, line);
        user = getWord(line, pos);
        if (user.empty() || (user[0] == '#')) {
            // Empty line or comment
            continue;
        }

        passwd = getWord(line, pos);
        access = getInt(line, pos);

        if (access < 0) {
            Log(Log::ERR) << "Malformed user record: " << line;
            continue;
        }

        g_UserDB.insert(std::make_pair(user, User(passwd, access)));
    };

    f.close();
}

Session::Session(const char* user, const std::string &connId)
    : m_Id((unsigned long)this), m_User(user), m_ConnId(connId)
{
    Refresh();
}

void Session::Refresh()
{
    m_UseTime = GetMonotonicTime() + ExpireTime;
}

std::ostream &operator<<(std::ostream& os, const Session& s)
{
    os << s.m_User << '@' << s.m_ConnId;
    return os;
}

unsigned int Authenticate(const char *user, const char *passwd)
{
    auto it = g_UserDB.find(user);

    if (it == g_UserDB.end() || it->second.m_Passwd != passwd) {
        Log(Log::ERR) << "User " << user << " failed to authenticate";
        return 0;
    } else {
        return it->second.m_Access;
    }
}

void RegisterSession(Session* s)
{
    g_Lock.lock();
    g_Sessions[s->m_Id] = s;
    g_Lock.unlock();

    Log(Log::INFO) << *s << " logged in";
}

Session *GetSession(unsigned long id)
{
    Session *s = nullptr;

    g_Lock.lock();

    auto it = g_Sessions.find(id);

    if (it != g_Sessions.end()) {
        s = it->second;
        s->Refresh();
    }

    g_Lock.unlock();
    return s;
}

void TerminateSession(Session *s)
{
    g_Lock.lock();
    g_Sessions.erase(s->m_Id);
    g_Lock.unlock();

    Log(Log::INFO) << *s << "logged out";

    delete s;
}

void CheckSessions()
{
    std::vector<Session*> expired;

    g_Lock.lock();

    for (auto it : g_Sessions) {
        Session* s = it.second;

        if (GetMonotonicTime() > s->m_UseTime) {
            g_Sessions.erase(s->m_Id);
            expired.push_back(s);
        }
    }

    g_Lock.unlock();

    for (Session* s : expired) {
        Log(Log::INFO) << *s << " expired";
        delete s;
    }
}
