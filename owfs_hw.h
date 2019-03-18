#include <string>

#include "hardware.h"

class OWFS_DS18xx : public Thermometer
{
public:
    OWFS_DS18xx(const char* path, float thresh);

protected:
    virtual float Measure();

private:
    std::string m_Path;

};
