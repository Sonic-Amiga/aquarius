#include <string>

#include "hardware.h"

class FileIOThermometer : public Thermometer
{
public:
    FileIOThermometer(const char* path, float thresh)
        : Thermometer(thresh), m_Path(path)
    {}

protected:
    virtual float Measure();

private:
    std::string m_Path;
};
