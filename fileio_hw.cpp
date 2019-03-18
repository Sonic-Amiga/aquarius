#include <math.h>

#include <fstream>
#include <string>

#include "fileio_hw.h"

float FileIOThermometer::Measure()
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
