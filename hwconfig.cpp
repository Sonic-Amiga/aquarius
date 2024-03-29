#include <libxml/parser.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hwconfig.h"
#include "hwstate.h"
#include "logging.h"

#ifdef _WIN32
static const char *const configPath = "C:\\aquarius\\etc\\aquarius\\config.xml";
#else
static const char *const configPath = "/etc/aquarius/config.xml";
#endif

/*
 * DeviceType instances are static and their constructors are run during early
 * program startup phase. Due to that we can't use any complex C++ constructs
 * like std::map for building our device type database because they might need
 * construction themselves. And we don't want to worry about constructors
 * ordering which might me system-specific.
 * Use something primitive, not requiring actual construction, like
 * single-linked list. No worry about performance since we are using it only
 * during XML config deserialization.
 */
static DeviceType *g_DeviceTypes = nullptr;

DeviceType::DeviceType(const char *type)
    : m_Type(type)
{
    m_Next = g_DeviceTypes;
    g_DeviceTypes = this;
}

static LoggerType* g_LoggerTypes = nullptr;

LoggerType::LoggerType(const char *type)
    : m_Type(type)
{
    m_Next = g_LoggerTypes;
    g_LoggerTypes = this;
}

std::ostream &operator<<(std::ostream& os, const xmlNode &node)
{
    os << " at " << node.doc->name << " line " << node.line;
    return os;
}

static int ReadInt(const char *str)
{
    char *p;
    int ret = strtol(str, &p, 0);

    return (*p == 0) ? ret : -1;
}

int GetIntProp(xmlNode *node, const char *name, int defVal)
{
    const char *str = GetStrProp(node, name);

    if (str) {
        int ret = ReadInt(str);

        if (ret == -1) {
	    Log(Log::ERR) << "Invalid value for \"" << name << "\" attribute " << *node;
	}

        return ret;
    } else {
	if (defVal == -1) {
	    Log(Log::ERR) << "Missing mandatory \"" << name << "\" attribute " << *node;
	}
        return defVal;
    }
}

float GetFloatProp(xmlNode *node, const char *name)
{
    const char *str = GetStrProp(node, name);

    if (str) {
        char *p;
        float ret = strtof(str, &p);

        if (*p == 0)
            return ret;
    }

    return NAN;
}

Hardware *HWConfig::GetDeviceProp(xmlNode *node, const char *name)
{
    const char *str = GetStrProp(node, name);

    if (str) {
        return GetHardware<Hardware>(str);
    } else {
        return nullptr;
    }
}

static int GetIntContent(xmlNode *node)
{
    for (xmlNode *n = node->children; n; n = n->next)
    {
        if (n->type == XML_TEXT_NODE) {
            return ReadInt((const char *)n->content);
        }
    }

    return -1;
}

void HWConfig::readNodes(xmlNode *startNode, const char *name, void(HWConfig::*parserFunc)(xmlNode *))
{
    xmlNode *cur_node;

    for (cur_node = startNode; cur_node; cur_node = cur_node->next) {
        if ((cur_node->type == XML_ELEMENT_NODE) &&
            !strcmp((const char *)cur_node->name, name))
        {
            (this->*parserFunc)(cur_node);
        }
    }
}

static void setId(Hardware *dev, xmlNode *node)
{
    const char *id = GetStrProp(node, "id");

    if (id)
        dev->m_name = id;
}

Hardware *HWConfig::createDevice(xmlNode *node)
{
    const char *type = GetStrProp(node, "type");
    DeviceType *dt;

    if (!type) {
        Log(Log::ERR) << "Malformed configuration element " << node->name;
        return nullptr;
    }

    for (dt = g_DeviceTypes; dt; dt = dt->m_Next) {
        if (!strcmp(dt->m_Type, type))
            break;
    }

    if (!dt) {
        Log(Log::ERR) << "Unknown device type " << type;
        return nullptr;
    }

    Hardware *dev = dt->CreateDevice(node, this);

    if (dev) {
        // Optional fields: id and description
        // id is required for referencing and status display
        setId(dev, node);

        // description will mostly be filled in automatically by logic components,
        // but for leak sensors it is supposed to describe the physical location
        const char *desc = GetStrProp(node, "description");
        if (desc)
            dev->m_description = desc;
    }

    return dev;
}

void HWConfig::createLogger(xmlNode* node)
{
    const char *type = GetStrProp(node, "type");
    LoggerType* lt;

    if (!type) {
        // Loggers are the first thing to instantiate; errors at this step
        // are practically critical, so we log them to console. Anyways we don't have
        // anything else at this point
        std::cerr << "Malformed configuration element " << node->name << std::endl;
        return;
    }

    for (lt = g_LoggerTypes; lt; lt = lt->m_Next) {
        if (!strcmp(lt->m_Type, type))
            break;
    }

    if (!lt) {
        std::cerr << "Unknown logger type " << type << std::endl;
        return;
    }

    LogListener *logger = lt->CreateLogger(node, this);

    m_Loggers.push_back(logger);
    AddLogListener(logger);
}

void HWConfig::createBus(xmlNode *node)
{
    m_Parent = createDevice(node);
    AddHardware(m_Parent);
    readNodes(node->children, "device", &HWConfig::createDeviceOnBus);
    m_Parent = nullptr;
}

void HWConfig::createDeviceOnBus(xmlNode *node)
{
    Hardware *dev = createDevice(node);
    AddHardware(dev);
}

void HWConfig::createHeater(xmlNode *heaterNode)
{
    xmlNode *node;

    for (node = heaterNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            const char *name = (const char *)node->name;
            Hardware *dev;

            if (!strcmp(name, "power_relay")) {
               dev = createDevice(node);
            } else if (!strcmp(name, "drain_relay")) {
               dev = createDevice(node);
            } else if (!strcmp(name, "pressure_switch")) {
               dev = createDevice(node);
            } else if (!strcmp(name, "temp_sensor")) {
               dev = createDevice(node);
            } else {
                Log(Log::ERR) << "Unknown heater controller component \"" << name << '"';
                continue;
            }

            // TODO: Properly create HeaterController here and connect inputs

            AddHardware(dev);
        }
    }
}

void HWConfig::createLeakDetector(xmlNode *heaterNode)
{
    xmlNode *node;

    for (node = heaterNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            const char *name = (const char *)node->name;
            Hardware *dev;

            if (!strcmp(name, "switch")) {
                dev = createDevice(node);
            } else {
                Log(Log::ERR) << "Unknown leak detector component \"" << name << '"';
                continue;
            }

            Switch *sw = dynamic_cast<Switch *>(dev);
            if (sw) {
                AddLeakSensor(sw);
            } else {
                Log(Log::ERR) << "Only switches are currently supported as leak sensor inputs";
            }
        }
    }
}

void HWConfig::createValveController(xmlNode *vcNode)
{
    Valve *CS = nullptr;
    Valve *HS = nullptr;
    Valve *HI = nullptr;
    Valve *HO = nullptr;
    Thermometer *HST = nullptr;
    int recoveryDelay = -1;
    xmlNode *node;

    for (node = vcNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            const char *name = (const char *)node->name;

            if (!strcmp(name, "cold_supply")) {
                CS = createValve(node);
                AddHardware(CS);
            } else if (!strcmp(name, "hot_supply")) {
                HS = createValve(node);
               AddHardware(HS);
            } else if (!strcmp(name, "heater_in")) {
                HI = createValve(node);
               AddHardware(HI);
            } else if (!strcmp(name, "heater_out")) {
                HO = createValve(node);
               AddHardware(HO);
            } else if (!strcmp(name, "hot_supply_temp")) {
                HST = createDeviceOfClass<Thermometer>(node);
               AddHardware(HST);
            } else if (!strcmp(name, "recovery_delay")) {
                recoveryDelay = GetIntContent(node);
            } else {
                Log(Log::ERR) << "Unknown valve controller component \""
                                << name << '"' << *node;
                continue;
            }
        }
    }

    if (recoveryDelay == -1) {
        Log(Log::ERR) << "Valve controller recovery delay is not specified" << *vcNode;
        return;
    }

    m_HWState = new HWState(this, CS, HS, HI, HO, HST, recoveryDelay);
}

Valve *HWConfig::createValve(xmlNode *vNode)
{
    int timeout = GetIntProp(vNode, "timeout");
    Relay *openRelay = nullptr;
    Relay *closeRelay = nullptr;
    Switch *openSwitch = nullptr;
    Switch *closedSwitch = nullptr;
    xmlNode *node;

    if (timeout == -1) {
        Log(Log::ERR) << "Invalid valve timeout value in the config";
        return nullptr;
    }

    for (node = vNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            const char *name = (const char *)node->name;

            if (!strcmp(name, "close_relay")) {
                closeRelay = dynamic_cast<Relay *>(createDevice(node));
            } else if (!strcmp(name, "open_relay")) {
                openRelay = dynamic_cast<Relay *>(createDevice(node));
            } else if (!strcmp(name, "closed_switch")) {
                closedSwitch = dynamic_cast<Switch *>(createDevice(node));
            } else if (!strcmp(name, "open_switch")) {
                openSwitch = dynamic_cast<Switch *>(createDevice(node));
            } else {
                Log(Log::ERR) << "Unknown valve component " << name;
            }

            // Note no AddHardware() here. Valve owns its components.
        }
    }

    // Relays are mandatory, switches are not. Cheap valve motors don't have them.
    if (closeRelay && openRelay) {
        Valve *v = new Valve(timeout, openRelay, closeRelay, openSwitch, closedSwitch);

        setId(v, vNode);
        return v;
    } else {
        Log(Log::ERR) << "Valve relay definition is invalid or missing";

        if (closeRelay) {
            delete closeRelay;
        }
        if (openRelay) {
            delete openRelay;
        }
        if (closedSwitch) {
            delete closedSwitch;
        }
        if (openSwitch) {
            delete openSwitch;
        }

        return nullptr;
    }
}

HWConfig::HWConfig()
    : m_HWState(nullptr)
{
    LIBXML_TEST_VERSION
    xmlDoc *doc = xmlReadFile(configPath, NULL, 0);

    if (doc == NULL) {
        fatal("Could not parse configuration file %s", configPath);
    }

    xmlNode *root = xmlDocGetRootElement(doc);
    xmlNode *cur_node = nullptr;
    xmlNode *startNode = nullptr;

    for (cur_node = root; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            startNode = cur_node->children;
            break;
        }
    }

    if (startNode) {
        readNodes(startNode, "logger", &HWConfig::createLogger);
        readNodes(startNode, "bus", &HWConfig::createBus);
        readNodes(startNode, "heater_controller", &HWConfig::createHeater);
        readNodes(startNode, "leak_detector", &HWConfig::createLeakDetector);
        // Valve controller interacts with all other components, so we create it
        // after everything else
        readNodes(startNode, "valve_controller", &HWConfig::createValveController);
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
}

HWConfig::~HWConfig()
{
    if (m_HWState)
        delete m_HWState;

    for (auto& hw : m_hw)
        delete hw.second;

    for (auto hw : m_LeakDetectors)
        delete hw;

    for (auto hw : m_AnonHW)
        delete hw;

    for (LogListener* logger : m_Loggers) {
        RemoveLogListener(logger);
        delete logger;
    }
}
