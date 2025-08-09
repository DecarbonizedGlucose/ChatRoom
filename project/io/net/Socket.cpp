#include "../include/Socket.hpp"
#include "../../global/include/logging.hpp"
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

Socket::~Socket() {
    if (fd >= 0) {
        close(fd);
    }
    fd = -1;
}

int Socket::get_fd() const {
    return fd;
}

std::string Socket::get_ip() const {
    return ip;
}

uint16_t Socket::get_port() const {
    return port;
}

void Socket::set_ip(const std::string& ip) {
    this->ip = ip;
}

void Socket::set_port(uint16_t port) {
    this->port = port;
}

void Socket::set_nonblocking(bool nonblock) {
    if (nonblock) {
        if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
            log_error("Failed to set socket to non-blocking: {}", strerror(errno));
        }
    } else {
        if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK) < 0) {
            log_error("Failed to set socket to blocking: {}", strerror(errno));
        }
    }
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

size_t DataSocket::get_read_buf_size() const {
    return read_buf.size();
}

size_t DataSocket::get_write_buf_size() const {
    return write_buf.size();
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
    if (size == 0) {
        return 0; // No data to send
    }
    ::write_size_to(fd, &size);
    // debug
    // std::cout << "Debug发送大小: " << std::dec << size << std::endl;
    // for (unsigned char c : write_buf) {
    //     std::cout << std::hex << (int)c << " ";
    // } std::cout << std::endl;
    return size + send(size);
}

bool DataSocket::send_protocol(const std::string& proto) {
    set_write_buf(proto);
    ssize_t res = send_with_size();
    if (res <= 0) {
        log_error("Failed to send protocol: {}", strerror(errno));
        return false; // Failed to send or no data
    }
    // log_info("Sent protocol of size {}", write_buf.size());
    return true;
}

bool DataSocket::receive_protocol(std::string& proto) {
    if (fd < 0) {
        return false; // Invalid socket
    }
    size_t size = 0;
    ssize_t received = ::read_size_from(fd, &size);
    if (received <= 0) {
        log_error("Failed to read size from socket: {}", strerror(errno));
        return false;
    }
    std::string tmp;
    ssize_t total = 0;
    while (total < size) {
        char buf[4096];
        ssize_t n = ::read(fd, buf, std::min(sizeof(buf), size - total));
        if (n <= 0) return false;
        tmp.append(buf, n);
        total += n;
    }
    proto = tmp;
    // debug
    // std::cout << "Debug收到大小: " << std::dec << proto.size() << std::endl;
    // for (unsigned char c : proto) {
    //     std::cout << std::hex << (int)c << " ";
    // } std::cout << std::endl;
    return true;
}

DataSocket::RecvState DataSocket::receive_protocol_with_state(std::string& proto) {
    if (fd < 0) return RecvState::Error; // Invalid socket

    // 1. 事件内循环读到EAGAIN, 拼接到packet_buf
    char tmp[4096];
    bool has_new_data = false;
    while (true) {
        ssize_t n = ::read(fd, tmp, sizeof(tmp));
        if (n > 0) {
            packet_buf.append(tmp, n);
            has_new_data = true;
        } else if (n == 0) {
            // 对端关闭
            return RecvState::Disconnected;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        } else {
            // 错误
            return RecvState::Error;
        }
    }

    // 2. 拆包
    if (packet_buf.size() < 4) {
        // 如果没有读到新数据且缓冲区不足4字节, 说明暂时没有完整包
        return RecvState::NoMoreData;
    }
    uint32_t net_len;
    memcpy(&net_len, packet_buf.data(), 4);
    size_t expected_size = ntohl(net_len);
    if (packet_buf.size() < 4 + expected_size) {
        // 包体不够, 需要等待更多数据
        return RecvState::NoMoreData;
    }
    proto = packet_buf.substr(4, expected_size);
    packet_buf.erase(0, 4 + expected_size);
    return RecvState::Success;
}

/* ----- AcceptedSocket ----- */

AcceptedSocket::AcceptedSocket(int fd, bool nonblock) : DataSocket(fd, nonblock) {
    if (fd < 0) {
        log_error("Tried to create AcceptedSocket with invalid fd: {}", fd);
        throw std::runtime_error("Failed to create AcceptedSocket");
    }
}

bool AcceptedSocket::disconnect() {
    if (fd < 0) {
        log_error("AcceptedSocket: Tried to disconnect without a valid connection");
        return false;
    }
    if (!connected) {
        return true;
    }
    if (close(fd) < 0) {
        log_error("AcceptedSocket: Failed to close socket: {}", strerror(errno));
        throw std::runtime_error("Failed to close socket: " + std::string(strerror(errno)));
    }
    fd = -1;
    connected = false;
    log_info("AcceptedSocket disconnected (fd:{})", fd);
    return true;
}

AcceptedSocket::~AcceptedSocket() {
    if (fd > 0) {
        disconnect();
    }
}

bool AcceptedSocket::is_connected() const {
    return connected;
}

/* ----- ConnectSocket ----- */

bool CSocket::connect() {
    if (connected) {
        log_error("CSocket: Tried to connect twice");
        return false;
    }
    if (fd < 0) {
        log_error("CSocket: Invalid socket");
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
    log_info("CSocket connected to {}:{}", ip, port);
    return true;
}

CSocket::~ConnectSocket() {
    if (fd > 0) {
        disconnect();
    }
}

bool CSocket::is_connected() const {
    return connected;
}

bool CSocket::disconnect() {
    if (fd < 0) {
        log_error("CSocket: Tried to disconnect without a valid connection");
        return false;
    }
    if (!connected) {
        return true;
    }
    if (close(fd) < 0) {
        log_error("CSocket: Failed to close socket: {}", strerror(errno));
        throw std::runtime_error("Failed to close socket: " + std::string(strerror(errno)));
    }
    fd = -1;
    connected = false;
    log_info("CSocket (fd:{}) disconnected from {}:{}", fd, ip, port);
    return true;
}

/* ----- ListenSocket ----- */

LSocket::ListenSocket(bool nonblock) : Socket(socket(AF_INET, SOCK_STREAM, 0), nonblock) {
    if (fd < 0) {
        log_error("Tried to create ListenSocket with invalid fd: {}", fd);
        throw std::runtime_error("Failed to create socket");
    }
}

LSocket::ListenSocket(const std::string& ip, uint16_t port, bool nonblock)
    : Socket(socket(AF_INET, SOCK_STREAM, 0), nonblock), ip(ip), port(port) {
    if (fd < 0) {
        log_error("Tried to create ListenSocket with invalid fd: {}", fd);
        throw std::runtime_error("Failed to create socket");
    }
}

bool LSocket::bind() {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        log_error("Invalid IP address: {}:{}", ip, port);
        return false;
    }
    int opt = 1;
    // 端口复用
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_error("设置端口复用{}:{}失败: {}", ip, port, strerror(errno));
        return false;
    }
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        log_error("Listen socket 绑定{}:{}失败: {}", ip, port, strerror(errno));
        return false;
    }
    this->binded = true;
    log_info("ListenSocket bound {}:{}", ip, port);
    return true;
}

bool LSocket::isBinded() const {
    return binded;
}

bool LSocket::listen() {
    if (::listen(fd, SOMAXCONN) < 0) {
        log_error("Failed to listen on ListenSocket: {}", strerror(errno));
        return false;
    }
    log_info("ListenSocket is now listening on {}:{}", ip, port);
    return true;
}

ASocket* LSocket::accept() {
    if (!binded) {
        throw std::runtime_error("Socket is not binded");
    }
    int client_fd = ::accept(fd, nullptr, nullptr);
    if (client_fd < 0) {
        log_error("Failed to accept connection: {}", strerror(errno));
        throw std::runtime_error("Failed to accept connection: " + std::string(strerror(errno)));
    }
    log_info("ListenSocket accepted connection on fd: {}", client_fd);
    ASocket* new_socket = new ASocket(client_fd, true);
    if (new_socket == nullptr) {
        log_error("Failed to create AcceptedSocket for fd: {}", client_fd);
        close(client_fd);
        throw std::runtime_error("Failed to create AcceptedSocket");
    }
    return new_socket;
}