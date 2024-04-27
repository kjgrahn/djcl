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

#include "version.h"
#include "error.h"
#include "server.h"
#include "timepoint.h"

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

    bool nonblock(int fd)
    {
	int n = fcntl(fd, F_GETFL, 0);
	if(n<0) return false;
	return fcntl(fd, F_SETFL, n | O_NONBLOCK) >= 0;
    }

    void ignore_sigpipe()
    {
	static struct sigaction ignore;
	ignore.sa_handler = SIG_IGN;
	ignore.sa_flags = SA_RESTART;
	(void)sigaction(SIGPIPE, &ignore, 0);
    }

    struct {
	bool sighup = false;
    } g;

    void sighup(int) { g.sighup = true; }

    void handle_sighup()
    {
	static struct sigaction sa;
	sa.sa_handler = sighup;
	sa.sa_flags = 0;
	(void)sigaction(SIGHUP, &sa, 0);
    }

    /* Change effective user, or complain and return false.
     */
    bool change_user(std::ostream& err,
		     const std::string& name)
    {
	const auto* e = getpwnam(name.c_str());
	if (!e) {
	    err << "error: user " << name << " not found\n";
	    return false;
	}
	const uid_t uid = e->pw_uid;
	if (seteuid(uid)) {
	    err << "error: seteuid(" << name << ") failed: "
		<< strerror(errno) << '\n';
	    return false;
	}
	return true;
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

	    fd = socket(r.ai_family, r.ai_socktype, r.ai_protocol);
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


    /**
     * The main event loop.
     */
    bool loop(const Content& content, const int lfd)
    {
	Server server(content, std::chrono::seconds {20});
	server.add(lfd);
	Periodic audit(now(), std::chrono::seconds{20});

	while(1) {
	    server.wait();
	    const Timepoint ts = now();

	    if (g.sighup) {
		Info(Syslog::log) << "SIGHUP; restarting server";
		g.sighup = false;
		return true;
	    }

	    for (Server::iterator i = server.lbegin(); i!=server.lend(); i++) {
		const Server::Event& ev = *i;
		if (ev.revents==0) continue;

		struct sockaddr_storage sa;
		socklen_t slen = sizeof sa;
		int fd = accept(ev.fd,
				reinterpret_cast<sockaddr*>(&sa), &slen);
		if(fd!=-1) {
		    nonblock(fd);
		    server.add(fd, sa, ts);
		}
	    }

	    for (Server::Event& ev : server) {
		if (ev.revents==0) continue;

		Session& session = server.session(ev);
		const int fd = ev.fd;
		Session::State state = Session::DIE;

		try {
		    if (writable(ev)) {
			state = session.write(fd, ts);
		    }
		    else if (readable(ev)) {
			state = session.read(fd, ts);
		    }
		}
		catch(const SessionError& e) {
		    Info(Syslog::log) << session << " aborted: " << e;
		}

		if(state==Session::DIE) {
		    server.remove(ev);
		}
		else {
		    server.ctl(ev, state);
		}
	    }

	    if(audit.check(ts)) {
		server.reconsider(ts);
	    }
	}
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
	    std::cout << "djcl " << version() << '\n'
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

    std::vector<std::string> files {argv+optind, argv+argc};

    if(hosts.empty() || root.empty() || files.empty()) {
	    std::cerr << usage << '\n';
	    return 1;
    }

    Files indices {begin(files), end(files), false};
    allergy::Index index {std::cerr, indices};
    if (!index.valid()) return 1;

    const Content content(std::cerr, hosts, index, root);
    if (!content.valid()) return 1;

    const int lfd = listening_socket(std::cerr, addr, port);
    if (lfd==-1) return 1;

    if (addr.empty()) addr = "*";
    Info(Syslog::log) << "listening on " << addr << ':' << port;

    ignore_sigpipe();
    handle_sighup();

    if(daemonize) {
	int err = daemon(0, 0);
	if(err) {
	    std::cerr << "error: failed to move to the background: "
		      << strerror(errno) << '\n';
	    return 1;
	}
	Syslog::log.activate();
    }

    while (loop(content, lfd)) {
	/* reestablish the server with a new Index.
	 */
	Files indices {begin(files), end(files), false};
	std::stringstream ss;
	index = {ss, indices};

	std::string s;
	while (std::getline(ss, s)) {
	    Info(Syslog::log) << s;
	}
    }

    return 0;
}
