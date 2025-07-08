#ifndef IOACTION_HPP
#define IOACTION_HPP
#include <unistd.h>
#include <vector>
#include <cerrno>
#include <cstdio>

namespace io {
    ssize_t err = ssize_t(-1);
}

ssize_t read_size_from(int fd, const size_t* const datasize) {
    if (fd < 0 || !datasize) {
        return io::err;
    }
    ssize_t n;
again:
    n = read(fd, (size_t*)datasize, sizeof(size_t));
    if (n == io::err) {
        if (errno == EINTR) {
            goto again;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            return io::err;
        }
    }
    return n;
}

ssize_t write_size_to(int fd, size_t* const datasize) {
    ssize_t n;
again:
    n = write(fd, (size_t*)datasize, sizeof(size_t));
    if (n == io::err) {
        if (errno == EINTR) {
            goto again;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            return io::err;
        }
    }
    return n;
}

ssize_t read_from(int fd, std::vector<char>& buf, size_t size = io::err) {
    if (fd < 0) {
        return io::err;
    }
    char tmp[BUFSIZ];
    ssize_t n;
    size_t total = 0;
    while (size == io::err || total < size) {
        size_t to_read = BUFSIZ;
        if (size == io::err) {
            to_read = std::min((size_t)BUFSIZ, size - total);
        }
        n = read(fd, tmp, to_read);
        if (n == io::err) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                return io::err;
            }
        } else if (n == 0) {
            return (ssize_t)total;
        }
        buf.insert(buf.end(), tmp, tmp + n);
        total += n;
        if (size != io::err && total >= size) {
            break;
        }
    }
}

ssize_t write_to(int fd, const std::vector<char>& buf, size_t size = io::err) {
    if (fd < 0) {
        return io::err;
    }
    ssize_t total_written = 0;
    ssize_t n;
    size_t to_write = (size == io::err) ? buf.size() : std::min(buf.size(), size);
    const char* data = buf.data();
    while ((size_t)total_written < to_write) {
        n = write(fd, data + total_written, to_write - total_written);
        if (n == io::err) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                return io::err;
            }
        }
        total_written += n;
    }
    return total_written;
}

#endif