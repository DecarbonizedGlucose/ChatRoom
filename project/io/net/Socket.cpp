#include "../include/Socket.hpp"
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

/* ----- Socket ----- */

Socket::Socket(int fd, bool nonblock) : fd(fd) {
    if (fd < 0) {
        throw std::runtime_error("Invalid file descriptor");
    }
    if (nonblock && fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        close(fd);
        this->fd = -1;
        throw std::runtime_error("Failed to set socket to non-blocking: " + std::string(strerror(errno)));
    }
}

Socket::Socket(Socket&& other) noexcept : fd(other.fd) {
    other.fd = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        fd = other.fd;
        other.fd = -1;
    }
    return *this;
}

/* ----- DataSocket ----- */

void DataSocket::get_read_buf(std::string& buf) {
    std::lock_guard<std::mutex> lock(read_mutex);
    buf.assign(read_buf.begin(), read_buf.end());
}

void DataSocket::set_write_buf(const std::string& buf) {
    std::lock_guard<std::mutex> lock(write_mutex);
    write_buf.assign(buf.begin(), buf.end());
}

ssize_t DataSocket::receive(size_t size) {
    if (fd < 0) {
        return -1;
    }
    ssize_t res; {
        std::lock_guard<std::mutex> lock(read_mutex);
        res = ::read_from(fd, read_buf, size);
    }
    return res;
}

ssize_t DataSocket::send(size_t size) {
    if (fd < 0) {
        return -1;
    }
    ssize_t res; {
        std::lock_guard<std::mutex> lock(write_mutex);
        res = ::write_to(fd, write_buf, size);
    }
    return res;
}

ssize_t DataSocket::send_with_size() {
    if (fd < 0) {
        return -1;
    }
    size_t size = write_buf.size();
    ssize_t sent = ::write_size_to(fd, &size);
    return send(size);
}

bool DataSocket::send_protocol(std::string& proto) {
    this->write_buf.assign(proto.begin(), proto.end());
    return send_with_size() > 0;
}

bool DataSocket::receive_protocol(std::string& proto) {
    if (fd < 0) {
        return false; // Invalid socket
    }
    size_t size = 0;
    ssize_t received = ::read_size_from(fd, &size);
    if (received <= 0) {
        return false; // Failed to read size or no data
    }
    read_buf.resize(size);
    received = receive(size);
    if (received <= 0) {
        return false; // Failed to read message content
    }
    proto.assign(read_buf.begin(), read_buf.end());
    return true;
}

/* ----- AcceptedSocket ----- */

/* ----- ConnectSocket ----- */

bool CSocket::connect() {
    if (connected) {
        return true; // Already connected
    }
    if (fd < 0) {
        return false; // Invalid socket
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid IP address: " + ip);
    }
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Failed to connect to " + ip + ":" + std::to_string(port) + " - " + std::string(strerror(errno)));
    }
    connected = true;
    return true;
}

bool CSocket::disconnect() {
    if (!connected || fd < 0) {
        return true; // Already disconnected
    }
    if (close(fd) < 0) {
        throw std::runtime_error("Failed to close socket: " + std::string(strerror(errno)));
    }
    fd = -1;
    connected = false;
    return true;
}

/* ----- ListenSocket ----- */

LSocket::ListenSocket(bool nonblock) : Socket(socket(AF_INET, SOCK_STREAM, 0), nonblock) {
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
}

LSocket::ListenSocket(const std::string& ip, uint16_t port, bool nonblock)
    : Socket(socket(AF_INET, SOCK_STREAM, 0), nonblock), ip(ip), port(port) {
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
}

bool LSocket::bind() {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        return false;
    }
    int opt = 1;
    // 端口复用
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        return false;
    }
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        return false;
    }
    this->binded = true;
    return true;
}

bool LSocket::listen() {
    if (::listen(fd, SOMAXCONN) < 0) {
        return false;
    }
    return true;
}

ASocketPtr LSocket::accept() {
    if (!binded) {
        throw std::runtime_error("Socket is not binded");
    }
    int client_fd = ::accept(fd, nullptr, nullptr);
    if (client_fd < 0) {
        throw std::runtime_error("Failed to accept connection: " + std::string(strerror(errno)));
    }
    return std::make_shared<ASocket>(client_fd);
}