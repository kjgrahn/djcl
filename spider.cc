#include "spider.h"

#include <array>
#include <sys/epoll.h>

Spider::Spider()
    : epfd(epoll_create(10))
{}

/* During loop(), monitor 'fd' for readability and call f(fd).
 *
 * There's no support for unregistering events. An fd gets removed
 * from the epoll set by the kernel when closed, and probably reused
 * soon after (at which point its Spider::ff map entry gets reused,
 * too).
 */
void Spider::read(int fd, std::function<void(int)> f)
{
    ff[fd] = f;

    epoll_event ev {};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

void Spider::loop()
{
    while (1) {
	std::array<epoll_event, 5> ev;
	const int n = epoll_wait(epfd, ev.data(), ev.size(), -1);

	for (int i=0; i < n; i++) {
	    const int fd = ev[i].data.fd;
	    auto it = ff.find(fd);
	    if (it != end(ff)) it->second(fd);
	}
    }
}
