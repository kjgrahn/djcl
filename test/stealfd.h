/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef ALLERGY_STEALFD_H_
#define ALLERGY_STEALFD_H_

#include <string>

/**
 * Replacing an fd (typically stdout or stderr) with a temporary file,
 * and restoring it on destruction. Before that, it's possible to
 * check what the process writes to the fd.
 *
 * It's really just a way to test class Syslog, in the state when it's
 * not yet writing to syslog.
 */
class Stealfd {
public:
    explicit Stealfd(int fd);
    ~Stealfd();
    std::string drain();

private:
    const int fd;
    const int backup;
};

#endif
