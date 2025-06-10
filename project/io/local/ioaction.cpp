#include "../include/ioaction.hpp"
#include <cerrno>
#include <cstdio>

ssize_t read_size_from(int fd, size_t* datasize) {
    if (fd < 0 || !datasize) {
        return io::err;
    }
    ssize_t n;
again:
    n = read(fd, datasize, sizeof(size_t));
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

ssize_t write_size_to(int fd, size_t* datasize) {
    ssize_t n;
again:
    n = write(fd, datasize, sizeof(size_t));
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

ssize_t read_from(int fd, std::vector<char>& buf) {
    if (fd < 0) {
        return io::err;
    }
    char tmp[BUFSIZ];
    ssize_t n;
    while (1) {
        n = read(fd, tmp, BUFSIZ);
        if (n == io::err) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                return io::err;
            }
        } else if (n == 0) {
            return buf.size();
        }
        buf.insert(buf.end(), tmp, tmp + n);
    }
}

ssize_t write_to(int fd, const std::vector<char>& buf) {
    if (fd < 0) {
        return io::err;
    }
    ssize_t total_written = 0;
    ssize_t n;
    size_t to_write = buf.size();
    const char* data = buf.data();
    while (total_written < (ssize_t)to_write) {
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
