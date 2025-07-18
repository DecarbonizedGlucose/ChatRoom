#pragma once

#include <unistd.h>
#include <string>

ssize_t read_size_from(int fd, const size_t* const datasize);
ssize_t write_size_to(int fd, size_t* const datasize);
ssize_t read_from(int fd, std::string& buf, size_t size = -1);
ssize_t write_to(int fd, const std::string& buf, size_t size = -1);
