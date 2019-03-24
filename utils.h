#include <time.h>

// Get monotonically increasing time in seconds
static inline time_t GetMonotonicTime()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}
