#include "../include/Socket.hpp"
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

/* ----- Socket ----- */

Socket::Socket(int fd) : fd(fd) {
    if (fd < 0) {
        throw std::runtime_error("Invalid file descriptor");
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
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

ssize_t DataSocket::receive(size_t size) {
    if (fd < 0) {
        return -1;
    }
    return ::read_from(fd, read_buf, size);
}

ssize_t DataSocket::send(size_t size) {
    if (fd < 0) {
        return -1;
    }
    return ::write_to(fd, write_buf, size);
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

ChatMessagePtr DataSocket::receive_message() {
    if (fd < 0) {
        return nullptr; // Invalid socket
    }
    std::string proto;
    if (!receive_protocol(proto)) {
        return nullptr; // Failed to receive protocol
    }
    auto message = std::make_shared<ChatMessage>();
    if (!message->ParseFromString(proto)) {
        return nullptr; // Failed to parse message
    }
    return message;
}

bool DataSocket::send_message(const ChatMessagePtr& chat_message) {
    std::string proto;
    chat_message->SerializeToString(&proto);
    return send_protocol(proto);
}

CommandPtr DataSocket::receive_command() {
    if (fd < 0) {
        return nullptr; // Invalid socket
    }
    std::string proto;
    if (!receive_protocol(proto)) {
        return nullptr; // Failed to receive protocol
    }
    auto command = std::make_shared<CommandRequest>();
    if (!command->ParseFromString(proto)) {
        return nullptr; // Failed to parse command
    }
    return command;
}

bool DataSocket::send_command(const CommandPtr& command) {
    std::string proto;
    command->SerializeToString(&proto);
    return send_protocol(proto);
}

// 收发文件？？不用json，用protocol
// TODO

// 发送大量(拉取)数据的函数？？
// TODO

/* ----- AcceptedSocket ----- */

/* ----- ConnectSocket ----- */

CSocket::ConnectSocket(const std::string& ip, uint16_t port)
    : DataSocket(socket(AF_INET, SOCK_STREAM, 0)) {
    this->ip = ip;
    this->port = port;
}

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

LSocket::ListenSocket() : Socket(socket(AF_INET, SOCK_STREAM, 0)) {
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
}

LSocket::ListenSocket(const std::string& ip, uint16_t port)
    : Socket(socket(AF_INET, SOCK_STREAM, 0)) {
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    bind(ip, port);
}

void LSocket::bind(const std::string& ip, uint16_t port) {
    this-> ip = ip;
    this->port = port;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid IP address: " + ip);
    }
    int opt = 1;
    // 端口复用
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Failed to set socket options: " + std::string(strerror(errno)));
    }
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Failed to bind socket to " + ip + ":" + std::to_string(port));
    }
    this->binded = true;
}

bool LSocket::listen() {
    if (::listen(fd, SOMAXCONN) < 0) {
        return false;
    }
    return true;
}

std::shared_ptr<ASocket> LSocket::accept() {
    if (!binded) {
        throw std::runtime_error("Socket is not binded");
    }
    int client_fd = ::accept(fd, nullptr, nullptr);
    if (client_fd < 0) {
        throw std::runtime_error("Failed to accept connection: " + std::string(strerror(errno)));
    }
    return std::make_shared<ASocket>(client_fd);
}