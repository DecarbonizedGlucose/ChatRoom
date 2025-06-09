#ifndef IOACTION_HPP
#define IOACTION_HPP
#include <unistd.h>
#include <string>

namespace io {
    ssize_t err = ssize_t(-1);
}

ssize_t read_size_from(int fd, size_t* datasize);

ssize_t read_from(int fd, std::string& buf);

ssize_t write_size_to(int fd, size_t* datasize);

ssize_t write_to(int fd, const std::string& buf);

#endif