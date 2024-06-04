/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef DJCL_PID_H
#define DJCL_PID_H

#include <sys/types.h>
#include <iostream>

/**
 * A slightly more convenient pid_t. Mostly, I dislike how -1 is
 * the error code (although I know why it is so.)
 */
class Pid {
public:
    Pid() : val{-1} {}
    Pid(pid_t val) : val{val} {}

    bool operator< (const Pid& other) const { return val < other.val; }
    explicit operator bool () const { return val != -1; }

    std::ostream& put(std::ostream& os) const { return os << '[' << val << ']'; }

    pid_t val;
};

inline
std::ostream& operator<< (std::ostream& os, const Pid& val)
{
    return val.put(os);
}

#endif
