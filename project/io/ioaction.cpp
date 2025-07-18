#include "include/ioaction.hpp"
#include <cerrno>
#include <cstdio>

ssize_t read_size_from(int fd, const size_t* const datasize) {
    if (fd < 0 || !datasize) {
        return -1;
    }
    ssize_t n;
again:
    n = read(fd, (size_t*)datasize, sizeof(size_t));
    if (n == -1) {
        if (errno == EINTR) {
            goto again;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            return -1;
        }
    }
    return n;
}

ssize_t write_size_to(int fd, size_t* const datasize) {
    ssize_t n;
again:
    n = write(fd, (size_t*)datasize, sizeof(size_t));
    if (n == -1) {
        if (errno == EINTR) {
            goto again;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            return -1;
        }
    }
    return n;
}

ssize_t read_from(int fd, std::string& buf, size_t size) {
    if (fd < 0) {
        return -1;
    }
    char tmp[BUFSIZ];
    ssize_t n;
    size_t total = 0;
    while (size == -1 || total < size) {
        size_t to_read = BUFSIZ;
        if (size != -1) {
            to_read = std::min((size_t)BUFSIZ, size - total);
        }
        n = read(fd, tmp, to_read);
        if (n == -1) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                return -1;
            }
        } else if (n == 0) {
            return (ssize_t)total;
        }
        buf.append(tmp, n);
        total += n;
        if (size != -1 && total >= size) {
            break;
        }
    }
    return total;
}

ssize_t write_to(int fd, const std::string& buf, size_t size) {
    if (fd < 0) {
        return -1;
    }
    ssize_t total_written = 0;
    ssize_t n;
    size_t to_write = (size == -1) ? buf.size() : std::min(buf.size(), size);
    const char* data = buf.data();
    while ((size_t)total_written < to_write) {
        n = write(fd, data + total_written, to_write - total_written);
        if (n == -1) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                return -1;
            }
        }
        total_written += n;
    }
    return total_written;
}
