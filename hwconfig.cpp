#include "hwconfig.h"
#include "fileio_hw.h"
#include "wiringpi_hw.h"

HWConfig::HWConfig()
{
    // Simple hardcoded conviguration

    // PCF8575, 16 bits
    m_ioExt1 = new I2C_pcf857x(new WPII2C(0x20), 16);

    // Hot water supply thermometer
    AddHardware("HST", new FileIOThermometer("/mnt/1wire/28.9C09E91B1302/temperature9", 45.0), "Hot supply");

    // Cold supply
    AddHardware("CS", new Valve(30,
                                new WPIRelay(0, true), new WPIRelay(1, true),
                                m_ioExt1->newSwitch(8, true), m_ioExt1->newSwitch(9, true)),
                                "Cold supply");
    // Hot supply
    AddHardware("HS", new Valve(30,
                                new WPIRelay(2, true), new WPIRelay(3, true),
                                m_ioExt1->newSwitch(10, true), m_ioExt1->newSwitch(11, true)),
                                "Hot supply");
    // Heater in
    AddHardware("HI", new Valve(30,
                                new WPIRelay(22, true), new WPIRelay(23, true),
                                m_ioExt1->newSwitch(12, true), m_ioExt1->newSwitch(13, true)),
                                "Heater in");
    // Heater out
    AddHardware("HO", new Valve(30,
                                new WPIRelay(24, true), new WPIRelay(25, true),
                                m_ioExt1->newSwitch(14, true), m_ioExt1->newSwitch(15, true)),
                                "Heater out");

    // Heater drain
    AddHardware("HD", new WPIRelay(13, true), "Heater drain");

    // Heater relay
    AddHardware("HR", new WPIRelay(12, true), "Heater relay");

    // Heater pressure switch
    AddHardware("HP", m_ioExt1->newSwitch(6, true), "Heater pressure");

    // Heater thermometer
    AddHardware("HT", new FileIOThermometer("/mnt/1wire/28.9C09E91B1301/temperature9", 45.0), "Heater");

    // Leak detectors
    AddLeakDetector("LD0", m_ioExt1->newSwitch(3, true), "Zone 0");
    AddLeakDetector("LD1", m_ioExt1->newSwitch(2, true), "Zone 1");
    AddLeakDetector("LD2", m_ioExt1->newSwitch(1, true), "Zone 2");
    AddLeakDetector("LD3", m_ioExt1->newSwitch(0, true), "Zone 3");
}

HWConfig::~HWConfig()
{
    unsigned int i;

    for (auto& hw : m_hw)
        delete hw.second;

    for (auto hw : m_LeakDetectors)
        delete hw;

    delete m_ioExt1;
}
