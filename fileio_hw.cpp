#include <math.h>

#include <fstream>
#include <string>

#include "fileio_hw.h"
#include "hwconfig.h"
#include "logging.h"

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

REGISTER_DEVICE_TYPE(FileIOThermometer)(xmlNode *node, HWConfig *)
{
    const char *path = GetStrProp(node, "path");
    float threshold = GetFloatProp(node, "threshold");

    if ((!path) || isnan(threshold)) {
        Log(Log::ERROR) << "Malformed FileIOThermometer description";
        return nullptr;
    }

    return new FileIOThermometer(path, threshold);
}
