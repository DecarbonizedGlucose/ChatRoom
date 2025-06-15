#ifndef IOACTION_HPP
#define IOACTION_HPP
#include <unistd.h>
#include <vector>

namespace io {
    ssize_t err = ssize_t(-1);
}

ssize_t read_from(int fd, std::vector<char>& buf, size_t size = io::err);

ssize_t write_to(int fd, const std::vector<char>& buf, size_t size = io::err);

#endif