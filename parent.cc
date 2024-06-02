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

    Pid spawn(Syslog& log, Command cmd, Pipe& stdout, Pipe& stderr)
    {
	const auto pid = fork();
	if (pid==-1) {
	    Err{log} << "cannot fork: " << std::strerror(errno);
	    return pid;
	}

	if (pid) {
	    stdout.parent();
	    stderr.parent();
	    return pid;
	}

	/* In the child process. As usual after forking, it's a matter
	 * of releasing resources that shouldn't be shared, setting up
	 * stdin/stdout/stderr, setting environment and $CWD, and
	 * lastly calling exec.
	 */
	stdout.child(1);
	stderr.child(2);

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

    template <class Key, class Val>
    void assign(std::map<Key, Val>& m, Key key, const Val& val)
    {
	m.insert_or_assign(key, val);
    }
}

/**
 * Constructor; includes trying to bring up the schedule, and
 * registering pipes in the Spider.
 */
Parent::Parent(const Schedule& schedule,
	       Syslog& log,
	       std::function<void(int, std::function<void(int)>)> reg)
    : schedule {schedule},
      log {log}
{
    for (const Command& cmd : schedule) {
	auto stdout = std::make_unique<Pipe>();
	auto stderr = std::make_unique<Pipe>();
	Pid pid = spawn(log, cmd, *stdout, *stderr);

	if (pid) {
	    assign(state, pid, cmd.name);
	    const int fdout = stdout->fd();
	    const int fderr = stderr->fd();

	    ss.erase(fdout);
	    ss.erase(fderr);

	    ss.emplace(fdout, Stream {cmd.name, "stdout", std::move(stdout)});
	    ss.emplace(fderr, Stream {cmd.name, "stderr", std::move(stderr)});

	    reg(fdout, [&] (int fd) { read(fd); });
	    reg(fderr, [&] (int fd) { read(fd); });
	}
    }
}

Parent::Stream::Stream(const Name& pname, const char* sname, std::unique_ptr<Pipe> pipe)
    : pname {pname},
      sname {sname},
      pipe {std::move(pipe)},
      text {"\n"}
{}

/**
 * Reap any children which have terminated. The name is a bit
 * misleading: the call doesn't block.
 *
 * The streams cannot sensibly be closed: the child might have forked
 * and some grandchild might still want to write.
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

/**
 * A stdout or stderr pipe has become readable, which might mean
 * there's new text on it, or that it has closed.
 */
void Parent::read(int fd)
{
    const auto it = ss.find(fd);
    if (it==end(ss)) return;
    Stream& stream = it->second;

    stream.text.feed(fd);

    char* a; char* b;
    while (stream.text.read(a, b)) {
	std::string s {a, b};
	if (s.size() && s.back()=='\n') s.pop_back();
	Info{log} << stream.pname << ": " << stream.sname << ": " << s;
    }

    if (stream.text.eof()) {
	Info{log} << stream.pname << ": " << stream.sname << ": EOF";
	ss.erase(it);
    }
}
