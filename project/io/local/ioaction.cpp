#include "../include/ioaction.hpp"

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

ssize_t read_from(int fd, std::string& buf) {
    if (fd < 0) {
        return io::err;
    }
    char tmp[BUFSIZ + 1];
    tmp[BUFSIZ] = '\0';
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
        buf += std::string(tmp, n);
    }
}

ssize_t write_to(int fd, const std::string& buf) {
    if (fd < 0) {
        return io::err;
    }
    ssize_t n = 0;
    while (1) {
        n = write(fd, buf.c_str() + n, buf.size());
        if (n == io::err) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                return io::err;
            }
        } else if (n < 0) {
            return io::err;
        }
        return buf.size();
    }
}
