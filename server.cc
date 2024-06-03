#include "server.h"

#include "split.h"

#include <sys/uio.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>

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

    /* Write a CRLF-terminated text to 'fd'.
     *
     * Should handle short writes, but doesn't -- we cheat and assume
     * such things don't happen on these low-bandwidth channels.
     */
    void writeln(int fd, const std::string& s)
    {
	std::array<iovec, 2> v;
	v[0] = {const_cast<char*>(&s[0]), s.size()};
	v[1] = {const_cast<char*>(crlf), 2};
	(void)writev(fd, v.data(), v.size());
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

    writeln(fd, "Hello. This is djcl; please type commands.");

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
    const auto it = ss.find(fd);
    if (it==end(ss)) return;
    Client& client = it->second;

    client.text.feed(fd);

    char* a; char* b;
    while (client.text.read(a, b)) {
	std::string s {a, b};
	auto rc = exec(s);
	if (rc.size()) writeln(fd, rc);
    }

    if (client.text.eof()) {
	Info{log} << "" << client.sa << ": connection closed by peer";
	ss.erase(it);
	::close(fd);
    }
}

/**
 * Execute a single textual command, which is a line of text.
 * If a string is returned, it's echoed to the client.
 */
std::string Server::exec(const std::string& s)
{
    const auto v = split(s, 2);
    if (v.empty()) return "";
    const auto& cmd = v[0];
    if (cmd=="start" && v.size()==2) {
	parent.start(v[1]);
	return "ok";
    }
    else if (cmd=="list") {
	const auto pp = parent.list();
	return join(crlf, pp);
    }
    else if (cmd=="die") {
    }
    else if (cmd=="exit") {
    }

    return "usage: start <program> | help | list | die | exit";
}
