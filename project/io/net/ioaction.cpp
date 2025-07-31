#include "../include/ioaction.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <arpa/inet.h>

ssize_t read_size_from(int fd, size_t* datasize) {
    if (fd < 0 || !datasize) {
        return -1;
    }
    uint32_t net_len = 0;
    char* ptr = reinterpret_cast<char*>(&net_len);
    size_t total = 0;
    while (total < sizeof(net_len)) {
        ssize_t n = read(fd, ptr + total, sizeof(net_len) - total);
        if (n == -1) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        if (n == 0) return 0;
        total += static_cast<size_t>(n);
    }
    *datasize = static_cast<size_t>(ntohl(net_len));
    return static_cast<ssize_t>(total);
}

ssize_t write_size_to(int fd, size_t* const datasize) {
    uint32_t net_len = htonl(static_cast<uint32_t>(*datasize));
    char* ptr = reinterpret_cast<char*>(&net_len);
    size_t total = 0;
    while (total < sizeof(net_len)) {
        ssize_t n = write(fd, ptr + total, sizeof(net_len) - total);
        if (n == -1) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        if (n == 0) return 0;
        total += static_cast<size_t>(n);
    }
    return static_cast<ssize_t>(total);
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
            if (errno == EINTR) {
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
    return (ssize_t)total;
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
