#ifndef TCPCONN_HPP
#define TCPCONN_HPP

#include <cstddef>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <cstdlib>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

class TcpConnection {
protected:
    std::string ip;
    uint16_t port = 0;
    struct sockaddr_in addr;
    int fd = -1;

public:
    TcpConnection();
    TcpConnection(std::string ip, uint16_t port, sa_family_t family = AF_INET);
    ~TcpConnection();

    int getfd() const;
};

class TcpData : public TcpConnection {
private:
    std::string buf;
    bool connected = false;

public:
    TcpData();
    TcpData(std::string ip, uint16_t port, sa_family_t family = AF_INET);
    TcpData(int fd);

    bool connect();
    bool disconnect();
    bool is_connected() const;
    ssize_t read_size(size_t* data_size);
    ssize_t write_size(size_t* data_size);
    ssize_t read_from();
    ssize_t write_to();
};

class TcpAcc : public TcpConnection {
private:
    bool listening = false;

public:
    TcpAcc();
    TcpAcc(std::string ip, uint16_t port, sa_family_t family = AF_INET);

    bool is_listening() const;
    bool set_listen();
    int new_connection();
};

using TC = TcpConnection;
using TCD = TcpData;
using TCA = TcpAcc;

#endif