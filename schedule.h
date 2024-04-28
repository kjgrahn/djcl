/* Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#ifndef DJCL_SCHEDULE_H
#define DJCL_SCHEDULE_H

#include <string>
#include <vector>

using Name = std::string;

/**
 * A named command to be fork+execed.
 */
struct Command {
    const Name name;
    std::vector<std::string> argv;
    std::vector<std::string> env;
    std::string cwd {"/"};

    explicit Command(const Name&);
    bool valid() const;
};

/**
 * The static list of processes ("programs" or "commands") we aim to
 * maintain.
 */
class Schedule {
public:
    Schedule(std::ostream& err, const std::string& file);
    bool valid() const { return !fail; }
    auto begin() const { return v.begin(); }
    auto end() const { return v.end(); }

private:
    std::vector<Command> v;
    bool fail = true;
};

#endif
