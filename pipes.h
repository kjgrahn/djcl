/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef DJCL_PIPES_H
#define DJCL_PIPES_H

/**
 * To be used with fork(2), to let the parent read the child's stdout
 * or stderr, separately and non-blocking. Will end up owning the read
 * end, on the parent side. On the child side it will be lost in
 * exec().
 */
class Pipe {
public:
    Pipe();
    ~Pipe();

    Pipe(const Pipe&) = delete;
    Pipe& operator= (const Pipe&) = delete;

    int fd() const { return rfd; }

    void parent();
    void child(int fd);

private:
    int rfd;
    int wfd;
};

#endif
