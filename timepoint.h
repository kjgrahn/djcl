/* -*- c++ -*-
 *
 * Copyright (c) 2021 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef ALLERGYD_TIMEPOINT_H_
#define ALLERGYD_TIMEPOINT_H_

#include <chrono>

/**
 * Good enough (in our context) for event loop timeouts, log messages,
 * and HTTP and file system timestamps.
 *
 * Although perhaps the system clock isn't good for event loops, since
 * it's not monotonic.
 */

using Duration  = std::chrono::milliseconds;
using Timepoint = std::chrono::time_point<std::chrono::system_clock,
					  Duration>;

using Days   = std::chrono::duration<int, std::ratio<3600*24>>;
using Months = std::chrono::duration<int, std::ratio<3600*24*30>>;
using Years  = std::chrono::duration<int, std::ratio<3600*24*365>>;

Timepoint now();

void time_of_day(char* buf, size_t len, const Timepoint t);

struct timespec;
Timepoint timepoint(const timespec&);

#endif
