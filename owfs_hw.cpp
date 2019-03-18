#include <math.h>

#include <fstream>
#include <string>

#include "owfs_hw.h"

OWFS_DS18xx::OWFS_DS18xx(const char* path, float thresh) :
    Thermometer(thresh),  m_Path(std::string(path) + "/temperature9")
{

}

float OWFS_DS18xx::Measure()
{
    std::fstream f(m_Path.c_str(), std::fstream::in);
    float val;

    if (f.good()) {
        f >> val;
    } else {
        val = NAN;
    }

    f.close();
    return val;
}
