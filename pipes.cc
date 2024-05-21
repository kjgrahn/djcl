#include "pipes.h"

#include "error.h"

#include <initializer_list>

#include <unistd.h>
#include <fcntl.h>

namespace {
    void cloexec(int fd)
    {
	fcntl(fd, F_SETFD, FD_CLOEXEC);
    }

    bool nonblock(int fd)
    {
	int n = fcntl(fd, F_GETFL, 0);
	if(n<0) return false;
	return fcntl(fd, F_SETFL, n | O_NONBLOCK) >= 0;
    }
}

Pipe::Pipe()
{
    int fds[2];
    int err;

    err = pipe2(fds, 0);
    if (err) throw FatalError {};
    rfd = fds[0];
    wfd = fds[1];

    cloexec(rfd);
    nonblock(rfd);
}

Pipe::~Pipe()
{
    for (int fd : { rfd, wfd }) {
	if (fd != -1) ::close(fd);
    }
}

/**
 * To be called once in the parent/reader end.
 */
void Pipe::parent()
{
    close(wfd); wfd = -1;
}

/**
 * To be called once in the child/write end; moves the pipe to a
 * certain fd (usually 1 or 2).
 */
void Pipe::child(int fd)
{
    close(rfd);
    dup2(wfd, fd);
    close(wfd);
}
