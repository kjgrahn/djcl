/*
 * Copyright (C) 2013, 2024 Jörgen Grahn.
 * All rights reserved.
 */
#include <log.h>

#include <split.h>
#include "stealfd.h"

#include <orchis.h>

namespace logs {

    using orchis::TC;

    /* True if a starts with b.
     */
    bool starts_with(const std::string& a,
		     const std::string& b)
    {
	auto res = std::mismatch(begin(a), end(a),
				 begin(b), end(b));
	return res.second==end(b);
    }

    /* An example string, n octets long.
     */
    std::string corpus(unsigned n)
    {
	std::string s = "0123456789";
	while (s.size() < n) {
	    s = s + s;
	}
	s.resize(n);
	return s;
    }

    std::string drain_log(Stealfd& sfd)
    {
	const auto v = split(sfd.drain(), 2);
	orchis::assert_eq(v.size(), 2);
	auto& s = v[1];
	return s;
    }

    void assert_log(Stealfd& sfd, const std::string& ref)
    {
	orchis::assert_eq(drain_log(sfd), ref);
    }

    void simple(TC)
    {
	Stealfd sfd {1};
	Info(Syslog::log) << "Hello, world!";
	assert_log(sfd, "Hello, world!");
    }

    void types(TC)
    {
	Stealfd sfd {1};
	Info(Syslog::log) << "foo" << "bar" << ' ' << 1 << 2;
	assert_log(sfd, "foobar 12");
    }

    void largish(TC)
    {
	Stealfd sfd {1};
	const auto s = corpus(200);
	Info(Syslog::log) << s;
	assert_log(sfd, s);
    }

    /* The log (before activate()) supports 200 but not 2000
     * characters in log messages, and truncates if needed.
     */
    void truncated(TC)
    {
	Stealfd sfd {1};
	const auto s = corpus(2000);
	Info(Syslog::log) << s;
	auto t = drain_log(sfd);
	orchis::assert_ge(t.size(), 200);
	orchis::assert_true(starts_with(s, t));

	// the overflow doesn't damage the log
	Info(Syslog::log) << "foo";
	assert_log(sfd, "foo");
    }
}
