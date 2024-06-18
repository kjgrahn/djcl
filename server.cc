#include "server.h"

#include "split.h"
#include "error.h"

#include <sys/uio.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

struct SocketBlockError : public FatalError {};

namespace {

    constexpr const char* crlf = "\r\n";

    std::ostream& operator<< (std::ostream& os, const sockaddr_storage& sa)
    {
	char hbuf[300];
	char sbuf[30];
	int rc = getnameinfo(reinterpret_cast<const struct sockaddr*>(&sa),
			     sizeof sa,
			     hbuf, sizeof hbuf,
			     sbuf, sizeof sbuf,
			     NI_NOFQDN
			     | NI_NUMERICHOST
			     | NI_NUMERICSERV);
	if(rc) {
	    return os << "unknown(" << std::strerror(errno) << ')';
	}
	return os << hbuf << ':' << sbuf;
    }

    int accept(int lfd, sockaddr_storage& sa)
    {
	socklen_t slen = sizeof sa;
	int fd = accept4(lfd,
			 reinterpret_cast<sockaddr*>(&sa), &slen,
			 SOCK_NONBLOCK | SOCK_CLOEXEC);
	return fd;
    }

    /* Write the contents of 'oss', ended with CRLF, to fd. Then empty
     * the stream so it can be reused. Throw a fatal error if the
     * write doesn't complete - we have no support for buffering while
     * sockets are blocked, and so on; it wouldn't be worth it.
     */
    void drain(int fd, std::ostringstream& oss)
    {
	oss << crlf;
	auto s = oss.str();
	oss.str("");
	ssize_t n = write(fd, s.data(), s.size());
	size_t len = n;
	if (n==-1 || len != s.size()) throw SocketBlockError {};
    }
}

Server::Server(Syslog& log, Spider& spider, Parent& parent)
    : log {log},
      spider {spider},
      parent {parent}
{}

/**
 * A listening socket is readable; let a client connect.
 */
void Server::connect(int lfd)
{
    sockaddr_storage sa;
    int fd = accept(lfd, sa);
    Info{log} << "new connection from " << sa;

    std::ostringstream greeting;
    greeting << "ok Hello. This is djcl; please type commands.";
    drain(fd, greeting);

    ss.erase(fd);
    ss.emplace(fd, Client{sa});

    spider.read(fd, [&] (int fd) { read(fd); });
}

Server::Client::Client(const sockaddr_storage& sa)
    : sa {sa},
      text {crlf}
{}

/**
 * A client socket is readable; read, and maybe find commands to
 * execute, or find out that the socket got closed.
 */
void Server::read(int fd)
{
    bool close = false;
    std::ostringstream resp;

    const auto it = ss.find(fd);
    if (it==end(ss)) return;
    Client& client = it->second;

    client.text.feed(fd);

    char* a; char* b;
    while (client.text.read(a, b)) {
	std::string s {a, b};
	if (!exec(resp, s)) close = true;
	drain(fd, resp);
    }

    if (client.text.eof()) {
	Info{log} << "" << client.sa << ": connection closed by peer";
	ss.erase(it);
	::close(fd);
    }
    else if (close) {
	Info{log} << "" << client.sa << ": closing connection";
	ss.erase(it);
	::close(fd);
    }
}

/**
 * Execute a single textual command, which is a line of text.
 *
 * Writes a textual response to 'os', but doesn't line-terminate it.
 * Returns false if the connection should close.
 */
bool Server::exec(std::ostream& os, const std::string& s)
{
    const auto v = split(s, 2);
    if (v.empty()) {
	os << "ok";
	return true;
    }

    const auto& cmd = v[0];
    if (cmd=="start") {
	if (v.size() > 1) {
	    parent.start(os, v[1]);
	}
	else {
	    parent.start_all(os);
	}
	return true;
    }

    if (cmd=="stop") {
	if (v.size() > 1) {
	    parent.stop(os, v[1]);
	}
	else {
	    parent.stop_all(os);
	}
	return true;
    }

    if (cmd=="list") {
	parent.list(os);
	os << "ok";
	return true;
    }

    if (cmd=="die") {
	spider.stop();
	os << "ok djcl exiting";
	return true;
    }

    if (cmd=="exit") {
	os << "ok closing connection";
	return false;
    }

    const auto rc = cmd=="help" ? "ok" : "error";

    os << rc << " usage:\n"
		"   start [name]\n"
		"   stop  [name]\n"
		"   list\n"
		"   help\n"
		"   die\n"
		"   exit";
    return true;
}
