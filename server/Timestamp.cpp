#include "Timestamp.h"

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#undef __STDC_FORMAT_MACROS

static_assert(sizeof(Timestamp) == sizeof(int64_t));

Timestamp::Timestamp()
    : microSeconds_(0)
{
}

Timestamp::Timestamp(int64_t microSeconds)
    : microSeconds_(microSeconds)
{
}

std::string Timestamp::toString() const
{
    char buf[32] = {0};
    int64_t seconds = microSeconds_ / kMicroSecondsPerSecond;
    int64_t microseconds = microSeconds_ % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf) - 1, "%ld.%06ld", seconds, microseconds);
    return buf;
}

std::string Timestamp::toFormateString() const
{
    char buf[64] = {0};
    time_t seconds = static_cast<time_t>(microSeconds_ / kMicroSecondsPerSecond);
    int64_t microseconds = microSeconds_ % kMicroSecondsPerSecond;
    struct tm tm_time;
    localtime_r(&seconds, &tm_time);

    snprintf(buf, sizeof(buf) - 1, "%4d-%02d-%02d %02d:%02d:%02d.%06ld",
        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
        microseconds);
    return buf;
}

Timestamp Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

Timestamp Timestamp::invalid()
{
    return Timestamp();
}

