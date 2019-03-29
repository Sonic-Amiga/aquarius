/*
 * Portability utilities.
 * Windows target is mostly used for development and testing, however it's
 * technically possible to target Windows IoT
 */
#ifndef UTILS_H
#define UTILS_H

#include <time.h>

#ifdef _WIN32

#include <Windows.h>

static inline time_t GetMonotonicTime()
{
	// WARNING!!! This works only for 49 days of uptime! Only for debugging!!!
	return GetTickCount() / 1000;
}

static inline unsigned int sleep(unsigned int seconds)
{
	Sleep(seconds * 1000);
	return 0;
}

static inline void msleep(unsigned int msec)
{
	Sleep(msec);
}

static inline struct tm *localtime_r(const time_t *timep, struct tm *result)
{
	localtime_s(result, timep);
	return result;
}

static inline unsigned int le32toh(unsigned int little_endian_32bits)
{
	return little_endian_32bits;
}

static inline unsigned int htole32(unsigned int host_32bits)
{
	return host_32bits;
}

#else

#include <endian.h>
#include <unistd.h>

// Sleep for given time in milliseconds.
// Windows loves milliseconds so we use this wrapper for small delays
static inline void msleep(unsigned int msec)
{
	usleep(msec * 1000);
}

// Get monotonically increasing time in seconds
static inline time_t GetMonotonicTime()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

#endif
#endif
