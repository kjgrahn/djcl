/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef DJCL_PARENT_H
#define DJCL_PARENT_H

#include "schedule.h"
#include "pid.h"
#include "log.h"

#include <map>

/**
 * The thing which forks and keeps track of forked processes.
 */
class Parent {
public:
    Parent(const Schedule& schedule, Syslog& log);

    void shutdown();
    void start(const Name&);
    void wait();

private:
    const Schedule& schedule;
    Syslog& log;

    std::map<Pid, Name> state;
};

#endif
