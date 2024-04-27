#include "parent.h"

#include "split.h"

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <cstring>
#include <iostream>

namespace {

    char* next(char*& p)
    {
	while(*p) p++;
	p++;
	return p;
    }

    constexpr char nil = '\0';

    /**
     * Constructing the argv for execvp(2).
     */
    class Argv {
    public:
	explicit Argv(const std::vector<std::string>& ss);
	Argv(const Argv&) = delete;
	Argv& operator= (const Argv&) = delete;

	std::vector<char*> val;

    private:
	std::string buf;
    };

    Argv::Argv(const std::vector<std::string>& ss)
	: val {ss.size() + 1, nullptr},
	  buf {join(nil, ss)}
    {
	char* p = &buf.front();
	for (unsigned i = 0; i < ss.size(); i++) {
	    val[i] = p;
	    next(p);
	}
    }
}

namespace {

    Pid spawn(Syslog& log, Command cmd)
    {
	const auto pid = fork();
	if (pid==-1) {
	    Err{log} << "cannot fork: " << std::strerror(errno);
	    return pid;
	}

	if (pid) {
	    return pid;
	}

	/* In the child process. As usual after forking, it's a matter
	 * of releasing resources that shouldn't be shared, setting up
	 * stdin/stdout/stderr, setting environment and $CWD, and
	 * lastly calling exec.
	 */
	for (auto& s : cmd.env) putenv(s.data());

	if (cmd.cwd.size() && chdir(cmd.cwd.c_str())) {
	    std::cerr << "error: " << cmd.name << ": cannot chdir to "
		      << cmd.cwd << ": "
		      << std::strerror(errno) << '\n';
	    std::exit(1);
	}

	const Argv argv {cmd.argv};
	execvp(argv.val[0], argv.val.data());

	std::cerr << "error: " << cmd.name << ": exec failed: "
		  << std::strerror(errno) << '\n';

	std::exit(1);
    }

    /* Render the information as e.g.
     *   exit 0
     *   exit 1
     *   aborted
     *   killed by SIGILL
     *   killed by signal 42
     */
    std::ostream& operator<< (std::ostream& os, const siginfo_t& si)
    {
	switch (si.si_code) {
	case CLD_EXITED: return os << "exit " << si.si_status;
	case CLD_KILLED:
	case CLD_DUMPED:
	    switch (si.si_status) {
	    case SIGABRT: return os << "aborted";
	    case SIGHUP:  return os << "killed by SIGHUP";
	    case SIGILL:  return os << "killed by SIGILL";
	    case SIGINT:  return os << "killed by SIGINT";
	    case SIGKILL: return os << "killed by SIGKILL";
	    case SIGSEGV: return os << "killed by SIGSEGV";
	    case SIGTERM: return os << "killed by SIGTERM";
	    default:      return os << "killed by signal " << si.si_status;
	    }
	default:
	    return os;
	}
    }
}

/**
 * Constructor; includes trying to bring up the schedule.
 */
Parent::Parent(const Schedule& schedule, Syslog& log)
    : schedule {schedule},
      log {log}
{
    for (const Command& cmd : schedule) {
	auto pid = spawn(log, cmd);
	if (pid) state[pid] = cmd.name;
    }
}

/**
 * Reap any children which have terminated. The name is a bit
 * misleading: the call doesn't block.
 */
void Parent::wait()
{
    siginfo_t info;
    while (waitid(P_ALL, 0, &info, WEXITED | WNOHANG) != -1 && info.si_pid) {

	const Pid pid {info.si_pid};
	auto it = state.find(pid);
	if (it != end(state)) {
	    const Name name = it->second;
	    state.erase(it);
	    Info{log} << pid << ' ' << name << ": " << info;
	}
	else {
	    /* Not sure why we'd be notified about a process
	     * terminating when we don't remember forking it, but
	     * anyway:
	     */
	    Warning{log} << pid << " (unknown): " << info;
	}
    }
}
