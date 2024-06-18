/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef DJCL_SPIDER_H
#define DJCL_SPIDER_H

#include <functional>
#include <map>

/**
 * A specialized wrapper around epoll(7). The name is mostly an inside
 * joke.
 *
 * Only supports acting on sockets being readable.
 *
 * The design might not be generally useful. There seems to be no
 * consensus on how to do this kind of thing, and I haven't been
 * comfortable using any of the ones I have used.
 */
class Spider {
public:
    Spider();
    Spider(const Spider&) = delete;

    void read(int fd, std::function<void(int)> f);
    void stop();

    void loop();

private:
    const int epfd;
    std::map<int, std::function<void(int)>> ff;
};

#endif
