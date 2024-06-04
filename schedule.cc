#include "schedule.h"

#include "split.h"

#include <fstream>
#include <cstring>
#include <algorithm>
#include <iterator>

Command::Command(const Name& name)
    : name{name}
{}

bool Command::valid() const
{
    return argv.size();
}

namespace {

    bool isws(unsigned char ch) { return std::isspace(ch); }

    template <class It> It ws(It a, It b)
    {
	while (a!=b) {
	    if (isws(*a)) break;
	    a++;
	}
	return a;
    }

    template <class It> It non_ws(It a, It b)
    {
	while (a!=b) {
	    if (!isws(*a)) break;
	    a++;
	}
	return a;
    }

    template <class It>
    std::string trim(It a, It b)
    {
	a = non_ws(a, b);
	while (a!=b) {
	    if (!isws(*std::prev(b))) break;
	    b--;
	}
	return {a, b};
    }

    /* Append an entry 'name', or use the last entry if it's 'name'.
     */
    Command& command(std::vector<Command>& v, const std::string& name)
    {
	if (v.empty() || v.back().name != name) return v.emplace_back(name);
	return v.back();
    }

    void exec(Command& p, const std::string& val)
    {
	p.argv = split(val);
    }

    void arg(Command& p, const std::string& val)
    {
	p.argv.push_back(val);
    }

    void cwd(Command& p, const std::string& val)
    {
	p.cwd = val;
    }

    void env(Command& p, const std::string& name, const std::string& val)
    {
	p.env.emplace_back(name + '=' + val);
    }

    bool invalid(const std::vector<Command>& v)
    {
	return std::none_of(begin(v), end(v), [] (auto p) { return p.valid(); });
    }
}

/**
 * Create from the specificed config file. Might write to 'err' and
 * generate a schedule that's not valid().
 */
Schedule::Schedule(std::ostream& err, const std::string& file)
{
    std::ifstream f {file};
    if (!f) {
	err << "error: cannot open " << file << " for reading: "
	    << std::strerror(errno) << '\n';
	return;
    }

    std::string s;
    while (std::getline(f, s)) {
	const char* a = s.c_str();
	const char* b = a + s.size();
	b = std::find(a, b, '#');
	auto c = std::find(a, b, '=');
	if (c==b) {
	    if (non_ws(a, b) != b) {
		err << "error: malformed config '" << s << "'\n";
	    }
	    continue;
	}

	/* " xxxx.yyyy  =  zz zzz zz  "
	 *  a    dd   e cc             b
	 */

	a = non_ws(a, c);
	auto d = std::find(a, c, '.');
	if (d==c) {
	    err << "error: malformed config '" << s << "'\n";
	    continue;
	}
	const std::string name {a, d++};
	auto e = ws(d, c++);
	const std::string param {d, e};
	const auto val = trim(c, b);

	Command& p = command(v, name);

	if (param=="exec")     exec(p, val);
	else if (param=="arg") arg(p, val);
	else if (param=="cwd") cwd(p, val);
	else                   env(p, param, val);
    }

    fail = v.empty() || invalid(v);
}

const Command* find(const Schedule& schedule, const Name& name)
{
    for (auto& cmd : schedule) {
	if (cmd.name==name) return &cmd;
    }
    return nullptr;
}
