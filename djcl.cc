/*
 * Copyright (c) 2010--2013, 2022, 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>

#include <getopt.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>

#include "error.h"
#include "timepoint.h"

#include "schedule.h"
#include "parent.h"
#include "sigpipe.h"
#include "spider.h"
#include "log.h"


namespace {

    bool reuse_addr(int fd)
    {
	int val = 1;
	return setsockopt(fd,
			  SOL_SOCKET, SO_REUSEADDR,
			  &val, sizeof val) == 0;
    }

    bool setbuf(int fd, int rx)
    {
	int err = setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
			     &rx, sizeof rx);
	return !err;
    }

    void ignore_sigpipe()
    {
	static struct sigaction ignore;
	ignore.sa_handler = SIG_IGN;
	ignore.sa_flags = SA_RESTART;
	(void)sigaction(SIGPIPE, &ignore, 0);
    }

    /* Create a listening socket on host:port (the wildcard address if
     * host is empty). Does everything including listen(), and prints
     * relevant error messages.
     */
    int listening_socket(std::ostream& err,
			 const std::string& host,
			 const std::string& port)
    {
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	struct addrinfo *result;
	const int s = getaddrinfo(host.empty()? 0: host.c_str(),
				  port.c_str(),
				  &hints, &result);
	if(s) {
	    err << "error: " << gai_strerror(s) << '\n';
	    return -1;
	}

	int fd = -1;
	const addrinfo* rp;
	for(rp = result; rp; rp = rp->ai_next) {
	    const addrinfo& r = *rp;

	    fd = socket(r.ai_family,
			r.ai_socktype | SOCK_CLOEXEC,
			r.ai_protocol);
	    if(fd == -1) continue;

	    if(reuse_addr(fd)
	       && bind(fd, r.ai_addr, r.ai_addrlen) == 0) {
		break;
	    }

	    close(fd);
	}

	freeaddrinfo(result);

	if(!rp || !setbuf(fd, 8192) || listen(fd, 10)==-1) {
	    err << "socket error: " << strerror(errno) << '\n';
	    return -1;
	}

	return fd;
    }

    namespace sigchld {

	Sigpipe pipe;

	void handler(int) { pipe.set(); }
    }

    void catch_sigchld()
    {
	struct sigaction sa = {};
	sa.sa_handler = sigchld::handler;
	sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
	(void)sigaction(SIGCHLD, &sa, nullptr);
    }
}


int main(int argc, char ** argv)
{
    const std::string prog = argv[0] ? argv[0] : "djcl";
    const std::string usage = "usage: "
	+ prog +
	" [-d]"
	" [-a listen-address]"
	" -p port"
	" -f config";
    const char optstring[] = "dp:a:f:";
    const struct option long_options[] = {
	{"daemon",       0, 0, 'd'},
	{"address",      1, 0, 'a'},
	{"port",         1, 0, 'p'},
	{"version", 	 0, 0, 'v'},
	{"help",    	 0, 0, 'h'},
	{0, 0, 0, 0}
    };

    bool daemonize = false;
    std::string addr;
    std::string port;
    std::string config;

    int ch;
    while((ch = getopt_long(argc, argv,
			    optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
	case 'd':
	    daemonize = true;
	    break;
	case 'a':
	    addr = optarg;
	    break;
	case 'p':
	    port = optarg;
	    break;
	case 'f':
	    config = optarg;
	    break;
	case 'h':
	    std::cout << usage << '\n';
	    return 0;
	case 'v':
	    std::cout << "djcl 1.0\n"
		      << "Copyright (c) 2024 Jörgen Grahn\n";
	    return 0;
	    break;
	case ':':
	case '?':
	    std::cerr << usage << '\n';
	    return 1;
	    break;
	default:
	    break;
	}
    }

    const std::vector<std::string> remaining {argv+optind, argv+argc};

    if (config.empty() || remaining.size()) {
	    std::cerr << usage << '\n';
	    return 1;
    }

    const Schedule schedule {std::cerr, config};
    if (!schedule.valid()) return 1;

    const int lfd = listening_socket(std::cerr, addr, port);
    if (lfd==-1) return 1;

    Syslog& log = Syslog::log;

    if (addr.empty()) addr = "*";
    Info(log) << "listening on " << addr << ':' << port;

    ignore_sigpipe();

    catch_sigchld();

    if(daemonize) {
	int err = daemon(0, 0);
	if(err) {
	    std::cerr << "error: failed to move to the background: "
		      << strerror(errno) << '\n';
	    return 1;
	}
	log.activate();
    }

    Spider spider;

    auto outf = [&] (auto fd, auto f) { spider.read(fd, f); };
    Parent parent {schedule, log, outf};

    spider.read(sigchld::pipe.readfd(),
		[&] (int) {
		    sigchld::pipe.drain();
		    parent.wait();
		});

    spider.loop();
    return 0;
}
