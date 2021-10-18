#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <string>

void SendEvent(const std::string& topic, int value);
void SendEvent(const std::string& topic, float value);

#endif
