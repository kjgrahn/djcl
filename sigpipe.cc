#include "sigpipe.h"

#include <fcntl.h>
#include <unistd.h>

Sigpipe::Sigpipe()
{
    int fd[2];
    int err = pipe2(fd, O_CLOEXEC | O_NONBLOCK);
    rfd = fd[0];
    wfd = fd[1];
}

Sigpipe::~Sigpipe()
{
    close(rfd);
    close(wfd);
}

void Sigpipe::set()
{
    write(wfd, ">", 1);
}

int Sigpipe::readfd() const
{
    return rfd;
}

void Sigpipe::drain()
{
    char buf[10];
    while (1) {
	ssize_t res = read(rfd, buf, sizeof buf);
	if (res==-1) break;
	size_t n = res;
	if (n != sizeof buf) break;
    }
}
