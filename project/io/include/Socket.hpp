#pragma once

#include <cstddef>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <memory>
#include <mutex>
#include "../../global/include/file.hpp"
#include "../include/ioaction.hpp"
#include "../../global/include/command.hpp"

class Socket;
class ListenSocket;
class DataSocket;
class AcceptedSocket;
class ConnectSocket;

class Socket {
protected:
    int fd = -1;

    std::string ip;
    uint16_t port = 0;
    sockaddr_in addr;

public:
    explicit Socket(int fd, bool nonblock = false);
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket&& other) noexcept;
    Socket(Socket&& other) noexcept;
    ~Socket();

    int get_fd() const;
    std::string get_ip() const;
    uint16_t get_port() const;
    void set_ip(const std::string& ip);
    void set_port(uint16_t port);
    void set_nonblocking(bool nonblock = true);
};

class DataSocket : public Socket {
protected:
    std::string read_buf;
    std::string write_buf;
    std::mutex read_mutex;
    std::mutex write_mutex;

    // 分包状态
    enum RecvPhase { READING_SIZE, READING_PAYLOAD };
    RecvPhase recv_phase = READING_SIZE;
    size_t expected_size = 0;
    std::string packet_buf; // 用于拼接半包/粘包

public:
    explicit DataSocket(int fd, bool nonblock = false) : Socket(fd, nonblock) {}

    void get_read_buf(std::string& buf);
    void set_write_buf(const std::string& buf);
    size_t get_read_buf_size() const;
    size_t get_write_buf_size() const;

    ssize_t receive(size_t size = -1);
    ssize_t send(size_t size = -1);
    ssize_t send_with_size();
    bool send_protocol(const std::string& proto);
    bool receive_protocol(std::string& proto);

    // 新增：事件内循环读, 自动处理粘包/半包
    enum class RecvState{
        Success,
        NoMoreData,
        Disconnected,
        Error
    };
    RecvState receive_protocol_with_state(std::string& proto);
};

class AcceptedSocket : public DataSocket {
private:
    bool connected = true;

public:
    AcceptedSocket() = delete;
    explicit AcceptedSocket(int fd, bool nonblock = false);
    ~AcceptedSocket();

    bool disconnect();
    bool is_connected() const;
};

class ConnectSocket : public DataSocket {
private:
    bool connected = false;
    std::string ip;
    uint16_t port = 0;

public:
    ConnectSocket() = delete;
    ConnectSocket(const std::string& ip, uint16_t port, bool nonblock = false)
        : DataSocket(socket(AF_INET, SOCK_STREAM, 0), nonblock), ip(ip), port(port) {}
    ~ConnectSocket();

    bool connect();
    bool disconnect();
    bool is_connected() const;
};

class ListenSocket : public Socket {
private:
    bool binded = false;
    std::string ip;
    uint16_t port = 0;

public:
    ListenSocket(bool nonblock = false);
    ListenSocket(const std::string& ip, uint16_t port, bool nonblock = false);

    bool bind();
    bool listen();
    bool isBinded() const;
    AcceptedSocket* accept();
};

using ASocket = AcceptedSocket;
using CSocket = ConnectSocket;
using LSocket = ListenSocket;

using SocketPtr = std::shared_ptr<Socket>;
using ASocketPtr = std::shared_ptr<AcceptedSocket>;
using CSocketPtr = std::shared_ptr<ConnectSocket>;
using LSocketPtr = std::shared_ptr<ListenSocket>;
