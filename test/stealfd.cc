#include "stealfd.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

Stealfd::Stealfd(int fd)
    : fd {fd},
      backup {dup(fd)}
{
    int tmp = open("/tmp/", O_TMPFILE | O_RDWR,
		   S_IRUSR | S_IWUSR);
    dup2(tmp, fd);
    close(tmp);
}

Stealfd::~Stealfd()
{
    dup2(backup, fd);
    close(backup);
}

/**
 * Pull all data from the tmpfile, then seek so that it can be filled
 * and drained again.
 */
std::string Stealfd::drain()
{
    lseek(fd, 0, SEEK_SET);
    std::string s;
    while (1) {
	char buf[100];
	ssize_t rc = read(fd, buf, sizeof buf);
	if (rc<0) break;
	size_t n = rc;
	s.append(buf, n);
	if (n < sizeof buf) break;
    }
    lseek(fd, 0, SEEK_SET);
    ftruncate(fd, 0);
    return s;
}
