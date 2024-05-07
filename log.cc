/*
 * Copyright (c) 2013, 2021 Jörgen Grahn
 * All rights reserved.
 *
 */
#include "log.h"

#include "timepoint.h"

#include <iostream>
#include <array>
#include <ctime>
#include <cstdio>

#include <sys/uio.h>
#include <sys/time.h>

namespace {

    auto timestamp()
    {
	const Timepoint ts = now();
	std::array<char, 13> v;
	time_of_day(v.data(), v.size(), ts);
	v.back() = ' ';
	return v;
    }
}

Syslog Syslog::log;

Syslog::Syslog()
    : os(this)
{
    char_type* p = &v[0];
    setp(p, p + v.size()-1);
    openlog("allergyd", 0, LOG_DAEMON);
}

Syslog::~Syslog()
{
    closelog();
}

void Syslog::flush(int prio)
{
    if (use_syslog) {
	*pptr() = '\0';
	syslog(prio, "%s", pbase());
    }
    else {
	char nl = '\n';
	auto header = timestamp();
	size_t len = pptr() - pbase();
	iovec v[3] = {{header.data(), header.size()},
		      {pbase(), len},
		      {&nl, 1}};
	writev(1, v, 3);
    }
    char_type* p = &v[0];
    setp(p, p + v.size()-1);
    os.clear();
}

void Syslog::activate()
{
    use_syslog = true;
}
