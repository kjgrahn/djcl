/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef DJCL_PARENT_H
#define DJCL_PARENT_H

#include "schedule.h"
#include "pid.h"
#include "pipes.h"
#include "textread.h"
#include "log.h"

#include <map>
#include <functional>
#include <memory>

/**
 * The thing which forks and keeps track of forked processes;
 * the core of djcl.
 */
class Parent {
public:
    Parent(const Schedule& schedule,
	   Syslog& log,
	   std::function<void(int, std::function<void(int)>)> outf);

    void shutdown();

    void start(std::ostream& os, const Name&);
    void start_all(std::ostream& os);
    void stop(std::ostream& os, const Name&) const;
    void stop_all(std::ostream& os) const;

    void list(std::ostream& os) const;

    void wait();
    void read(int fd);

private:
    const Schedule& schedule;
    Syslog& log;
    const std::function<void(int, std::function<void(int)>)> reg;

    std::map<Pid, Name> state;

    struct Stream {
	Stream(const Name& pname, const char* sname, std::unique_ptr<Pipe> pipe);
	Stream(Stream&&) = default;
	Stream(const Stream&) = delete;

	const Name pname;
	const char* const sname;
	std::unique_ptr<Pipe> pipe;
	sockutil::TextReader text;
    };

    std::map<int, Stream> ss;

    Pid start(const Command&);
};

#endif
