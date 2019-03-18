#ifndef USERDB_H
#define USERDB_H

#include <time.h>
#include <ostream>
#include <string>

class Session
{
public:
    Session(const char *user, const std::string &connId);
    virtual ~Session() {}
    void Refresh();

    unsigned long m_Id;
    time_t        m_UseTime;
    std::string   m_User;
    std::string   m_ConnId;
};

std::ostream &operator<<(std::ostream& os, const Session& s);

void InitUserDB();
unsigned int Authenticate(const char *user, const char *passwd);
void RegisterSession(Session* s);
Session* GetSession(unsigned long id);
void TerminateSession(Session *s);
void CheckSessions();

#endif
