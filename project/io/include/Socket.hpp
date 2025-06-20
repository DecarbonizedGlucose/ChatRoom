#ifndef SOCKET_HPP
#define SOCKET_HPP

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
#include "../../global/include/file.hpp"
#include "../../global/include/message.hpp"
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

public:
    explicit Socket(int fd);
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket&& other) noexcept;
    Socket(Socket&& other) noexcept;
    virtual ~Socket() = 0;

    int getFd() const { return fd; }
    virtual void bind(const std::string& ip, uint16_t port) = 0;
    virtual bool listen() = 0;
};

class DataSocket : public Socket {
protected:
    std::vector<char> buf;
    std::string ip;
    uint16_t port = 0;
    sockaddr_in addr;

public:
    explicit DataSocket(int fd) : Socket(fd) {}

    // 下面这几个接口用于收发消息(Message)
    ssize_t receive(size_t size = io::err);
    ssize_t send(size_t size = io::err);

    MesPtr receive_header();
    bool send_header(const MesPtr& message);

    MesPtr receive_message();
    bool send_message(const MesPtr& message); // todo

    // 用在文件类消息收发函数内部
    bool send_file(const FilePtr& file); // todo
    bool receive_file(const FilePtr& file); // todo

    bool send_command(const ComPtr& command); // todo
    ComPtr receive_command(); // todo

    void bind(const std::string& ip, uint16_t port) override final {}
    bool listen() override final {}
};

class AcceptedSocket : public DataSocket {
public:
    AcceptedSocket() = delete;
    explicit AcceptedSocket(int fd) : DataSocket(fd) {}
};

class ConnectSocket : public DataSocket {
private:
    bool connected = false;

public:
    ConnectSocket() = delete;
    ConnectSocket(const std::string& ip, uint16_t port);

    bool connect();
    bool disconnect();
    bool is_connected() const { return connected; }
};

class ListenSocket : public Socket {
private:
    std::string ip;
    uint16_t port = 0;
    sockaddr_in addr;
    bool binded = false;

public:
    ListenSocket();
    ListenSocket(const std::string& ip, uint16_t port);

    void bind(const std::string& ip, uint16_t port) override;
    bool listen();
    bool isBinded() const { return binded; }
    std::shared_ptr<AcceptedSocket> accept();
};

using ASocket = AcceptedSocket;
using CSocket = ConnectSocket;
using LSocket = ListenSocket;

using pSocket = std::shared_ptr<Socket>;
using pASocket = std::shared_ptr<AcceptedSocket>;
using pCSocket = std::shared_ptr<ConnectSocket>;
using pLSocket = std::shared_ptr<ListenSocket>;

#endif