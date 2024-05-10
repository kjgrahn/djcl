/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef DJCL_SIGPIPE_H
#define DJCL_SIGPIPE_H

/**
 * The usual roundtripping mechanism through the kernel, to turn e.g.
 * a signal into a poll event.
 *
 * The name is a pun; it's a pipe for signals, not a signal about
 * pipes.
 */
class Sigpipe {
public:
    Sigpipe();
    ~Sigpipe();

    void set();
    int readfd() const;
    void drain();

private:
    int rfd;
    int wfd;
};

#endif
