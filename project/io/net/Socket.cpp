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

// ssize_t DataSocket::receive_size(size_t* data_size) {
//     if (fd < 0 || !data_size) {
//         return -1;
//     }
//     return ::read_size_from(fd, data_size);
// }

// ssize_t DataSocket::send_size(size_t* data_size) {
//     if (fd < 0 || !data_size) {
//         return -1;
//     }
//     return ::write_size_to(fd, data_size);
// }

ssize_t DataSocket::receive(size_t size) {
    if (fd < 0) {
        return -1;
    }
    return ::read_from(fd, buf, size);
}

ssize_t DataSocket::send(size_t size) {
    if (fd < 0) {
        return -1;
    }
    return ::write_to(fd, buf, size);
}

MesPtr DataSocket::receive_header() {
    if (fd < 0) {
        return nullptr;
    }
    char type = 0;
    // 头部全读出来
    receive(33);
    type = buf[0];
    MesPtr message;
    switch (type) {
    case 'T':
        message = std::make_shared<TMes>();
        break;
    case 'F':
        message = std::make_shared<FMes>();
        break;
    case 'I':
        message = std::make_shared<IMes>();
        break;
    case 'C':
        message = std::make_shared<CMes>();
        break;
    default:
        return nullptr;
    }
    if (message == nullptr) {
        return nullptr;
    }
    std::memcpy(&message->timestamp, buf.data() + 1, sizeof(message->timestamp));
    std::memcpy(&message->sender, buf.data() + 9, sizeof(message->sender));
    std::memcpy(&message->receiver, buf.data() + 17, sizeof(message->receiver));
    std::memcpy(&message->size, buf.data() + 25, sizeof(message->size));
    return message;
}

bool DataSocket::send_header(const MesPtr& message) {
    if (fd < 0 || !message) {
        return false;
    }
    buf.resize(33);
    buf[0] = message->type; // 消息类型
    std::memcpy(buf.data() + 1, &message->timestamp, sizeof(message->timestamp));
    std::memcpy(buf.data() + 9, &message->sender, sizeof(message->sender));
    std::memcpy(buf.data() + 17, &message->receiver, sizeof(message->receiver));
    std::memcpy(buf.data() + 25, &message->size, sizeof(message->size));
    return send(33) >= 0;
}

MesPtr DataSocket::receive_message() {
    MesPtr message = receive_header();
    if (!message) {
        return nullptr; // 读取消息头失败
    }
    switch (message->type) {
    case 'T': {
            auto text_message = std::static_pointer_cast<TMes>(message);
            if (receive(text_message->size) < 0) {
                message.reset();
                text_message.reset();
                return nullptr; // 读取文本消息内容失败
            }
            text_message->content.assign(buf.data(), text_message->size);
            return text_message;
        }
    case 'F': {
            auto file_message = std::static_pointer_cast<FMes>(message);
            if (receive(file_message->size) < 0) {
                message.reset();
                file_message.reset();
                return nullptr; // 读取文件消息内容失败
            }
            size_t name_size;
            receive(sizeof(size_t));
            std::memcpy(&name_size, buf.data(), sizeof(size_t));
            receive(name_size);
            file_message->file_name.assign(buf.data(), name_size);
            return file_message;
        }
    case 'I':
    case 'C':
    default:
        return nullptr;
    }
}

bool DataSocket::send_message(const MesPtr& message) {

}

bool DataSocket::send_file(const FilePtr& file) {

}

bool DataSocket::receive_file(const FilePtr& file) {
    
}


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