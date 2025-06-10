#include "../include/TcpConnection.hpp"
#include "../include/ioaction.hpp"
#include <stdexcept>
#include <cstring>

/* ----- TcpConnection ----- */

TC::TcpConnection() {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)) + '\n');
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        close(fd);
        fd = -1;
        throw std::runtime_error("Failed to set socket to non-blocking: " + std::string(strerror(errno)) + '\n');
    }
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
}

TC::TcpConnection(std::string ip, uint16_t port, sa_family_t family)
    : ip(std::move(ip)), port(port) {
    fd = socket(family, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)) + '\n');
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        close(fd);
        fd = -1;
        throw std::runtime_error("Failed to set socket to non-blocking: " + std::string(strerror(errno)) + '\n');
    }
    addr.sin_family = family;
    addr.sin_port = htons(port);
    if (inet_pton(family, this->ip.c_str(), &addr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid IP address: " + this->ip + " - " + std::string(strerror(errno)) + '\n');
    }
}

TC::~TcpConnection() {
    if (fd >= 0) {
        close(fd);
    }
    fd = -1;
}

int TC::getfd() const {
    return fd;
}

/* ----- TcpDataent ----- */

TCD::TcpData() : TcpConnection() {
    buf.resize(BUFSIZ);
}

TCD::TcpData(std::string ip, uint16_t port, sa_family_t family)
    : TcpConnection(std::move(ip), port, family) {
    buf.resize(BUFSIZ);
}

TCD::TcpData(int fd) {
    this->fd = fd;
    if (fd < 0) {
        throw std::runtime_error("Invalid file descriptor: " + std::to_string(fd) + '\n');
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        close(fd);
        this->fd = -1;
        throw std::runtime_error("Failed to set socket to non-blocking: " + std::string(strerror(errno)) + '\n');
    }
    buf.resize(BUFSIZ);
    connected = true;
}

bool TCD::connect() {
    if (connected) {
        return true; // Already connected
    }
    if (fd < 0) {
        return false;
    }
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Failed to connect to " + ip + ":" + std::to_string(port) + " - " + std::string(strerror(errno)) + '\n');
    }
    connected = true;
    return true;
}

bool TCD::disconnect() {
    if (!connected || fd < 0) {
        return true;
    }
    if (close(fd) < 0) {
        throw std::runtime_error("Failed to close socket: " + std::string(strerror(errno)) + '\n');
    }
    fd = -1;
    connected = false;
    return true;
}

bool TCD::is_connected() const {
    return connected;
}

ssize_t TCD::read_size(size_t* data_size) {
    if (fd < 0 || !data_size) {
        return -1;
    }
    return ::read_size_from(fd, data_size);
}

ssize_t TCD::write_size(size_t* data_size) {
    if (fd < 0 || !data_size) {
        return -1;
    }
    return ::write_size_to(fd, data_size);
}

ssize_t TCD::read_from() {
    if (fd < 0) {
        return -1;
    }
    return ::read_from(fd, buf);
}

ssize_t TCD::write_to() {
    if (fd < 0) {
        return -1;
    }
    return ::write_to(fd, buf);
}

/* ----- TcpAccver ----- */

TCA::TcpAcc() : TcpConnection() {}

TCA::TcpAcc(std::string ip, uint16_t port, sa_family_t family)
    : TcpConnection(std::move(ip), port, family), listening(false) {}

bool TCA::set_listen() {
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Failed to set socket options: " + std::string(strerror(errno)) + '\n');
    }
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)) + '\n');
    }
    if (listen(fd, SOMAXCONN) < 0) {
        throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)) + '\n');
    }
    listening = true;
    return true;
}