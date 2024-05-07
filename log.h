/* -*- c++ -*-
 *
 * Copyright (c) 2013 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef OUTN_LOG_H_
#define OUTN_LOG_H_

#include <iostream>
#include <streambuf>
#include <array>

#include <syslog.h>


/**
 * Crude ostream interface to stderr or syslog(3).  We want an
 * efficient way of saying something like:
 *
 *    LOG_WARNING << "session " << s << " closed unexpectedly";
 *
 * and have it translate to one syslog message.
 *
 * Since a daemon should log to stderr until it goes into the
 * background, this class does too, until actually told to switch to
 * syslog. In that case it adds a simple timestamp.
 *
 * Class Syslog implements the ostream, a fixed, limited-size stream buffer,
 * and the syslogging.  The Log<Prio> template makes the syntax acceptable.
 * Overlong messages will simply be truncated.
 */
class Syslog : private std::basic_streambuf<char> {
public:
    Syslog();
    ~Syslog();
    std::ostream& ostream() { return os; }
    void flush(int prio);

    void activate();

    /**
     * Since the syslog is naturally process-global and reentrancy is
     * irrelevant here, we might as well provide a global Syslog object.
     */
    static Syslog log;

private:
    std::array<char_type, 500> v;
    std::ostream os;
    bool use_syslog = false;
};

/**
 * Generating a log message with a certain prio through a Syslog
 * object.
 */
template <int Prio>
struct Log {
    explicit Log(Syslog& syslog) : syslog(syslog) {}
    ~Log() { syslog.flush(Prio); }

    Log(const Log&) = delete;
    Log& operator= (const Log&) = delete;

    template <class T>
    std::ostream& operator<< (const T& t) {
	return syslog.ostream() << t;
    }

private:
    Syslog& syslog;
};

typedef Log<LOG_EMERG> Emerg;
typedef Log<LOG_ALERT> Alert;
typedef Log<LOG_CRIT> Crit;
typedef Log<LOG_ERR> Err;
typedef Log<LOG_WARNING> Warning;
typedef Log<LOG_NOTICE> Notice;
typedef Log<LOG_INFO> Info;
typedef Log<LOG_DEBUG> Debug;

#endif
