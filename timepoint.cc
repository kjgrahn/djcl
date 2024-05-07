/* Copyright (c) 2021 Jörgen Grahn
 * All rights reserved.
 *
 */
#include "timepoint.h"

#include <time.h>
#include <cstdio>

Timepoint now()
{
    auto t = std::chrono::system_clock::now();
    return std::chrono::time_point_cast<Duration>(t);
}

/**
 * Current (local) time as just time of day, e.g. "06:02:00.999".
 * Note the milliseconds; it's for for debug logging.
 *
 */
void time_of_day(char* buf, size_t len, const Timepoint t)
{
    const time_t tt = std::chrono::system_clock::to_time_t(t);
    const unsigned ms = t.time_since_epoch().count() % 1000;

    const struct tm tm = *std::localtime(&tt);
    std::snprintf(buf, len, "%02d:%02d:%02d.%03u",
		  tm.tm_hour, tm.tm_min, tm.tm_sec, ms);

}

/**
 * Conversion, with truncation to milliseconds.
 */
Timepoint timepoint(const timespec& ts)
{
    const auto t0 = std::chrono::system_clock::from_time_t(ts.tv_sec);
    const auto ns = std::chrono::nanoseconds(ts.tv_nsec);
    return std::chrono::time_point_cast<Duration>(t0 + ns);
}
