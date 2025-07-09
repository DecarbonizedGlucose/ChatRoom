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

    ssize_t receive(size_t size = io::err);
    ssize_t send(size_t size = io::err);
    ssize_t send_with_size();
    bool send_protocol(std::string& proto);
    bool receive_protocol(std::string& proto);

    ChatMessagePtr receive_message();
    bool send_message(const ChatMessagePtr& message);

    // 用在文件类消息收发函数内部
    //bool receive_file(const ChatFilePtr& file); // 不是这么玩的
    //bool send_file(const ChatFilePtr& file);

    CommandPtr receive_command();
    bool send_command(const CommandPtr& command);

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

using SocketPtr = std::shared_ptr<Socket>;
using ASocketPtr = std::shared_ptr<AcceptedSocket>;
using CSocketPtr = std::shared_ptr<ConnectSocket>;
using LSocketPtr = std::shared_ptr<ListenSocket>;

#endif