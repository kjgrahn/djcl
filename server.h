/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef DJCL_SERVER_H
#define DJCL_SERVER_H

#include "parent.h"
#include "spider.h"
#include "textread.h"
#include "log.h"

#include <map>
#include <functional>

#include <sys/socket.h>

/**
 * TCP server for the user interface, where you can tell djcl to start
 * programs, and stuff.
 */
class Server {
public:
    Server(Syslog& log, Spider& spider, Parent& parent);

    void connect(int lfd);
    void read(int fd);

private:
    Syslog& log;
    Spider& spider;
    Parent& parent;

    std::string exec(const std::string& s);

    struct Client {
	explicit Client(const sockaddr_storage& sa);
	Client(Client&&) = default;

	const sockaddr_storage sa;
	sockutil::TextReader text;
    };

    std::map<int, Client> ss;
};

#endif
