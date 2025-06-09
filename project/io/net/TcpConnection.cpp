#include "../include/TcpConnection.hpp"
#include "../include/ioaction.hpp"
#include <stdexcept>
#include <cstring>

TC::TcpConnection() {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)) + '\n');
    }
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    buf.resize(BUFSIZ);
}

TC::TcpConnection(std::string ip, uint16_t port, sa_family_t family)
    : ip(std::move(ip)), port(port) {
    fd = socket(family, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)) + '\n');
    }
    addr.sin_family = family;
    addr.sin_port = htons(port);
    if (inet_pton(family, this->ip.c_str(), &addr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid IP address: " + this->ip + " - " + std::string(strerror(errno)) + '\n');
    }
    buf.resize(BUFSIZ);
}

bool TC::connect() {
    if (fd < 0) {
        return false;
    }
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Failed to connect to " + ip + ":" + std::to_string(port) + " - " + std::string(strerror(errno)) + '\n');
    }
    return true;
}

ssize_t TC::read_size(size_t* data_size) {
    if (fd < 0 || !data_size) {
        return -1;
    }
    return ::read_size_from(fd, data_size);
}

ssize_t TC::write_size(size_t* data_size) {
    if (fd < 0 || !data_size) {
        return -1;
    }
    return ::write_size_to(fd, data_size);
}

ssize_t TC::read_from() {
    if (fd < 0) {
        return -1;
    }
    return ::read_from(fd, buf);
}

ssize_t TC::write_to() {
    if (fd < 0) {
        return -1;
    }
    return ::write_to(fd, buf);
}