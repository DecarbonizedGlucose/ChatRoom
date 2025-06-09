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
private:
    std::string ip;
    uint16_t port = 0;
    struct sockaddr_in addr;
    int fd = -1;
    std::string buf;
public:
    TcpConnection();
    TcpConnection(std::string ip, uint16_t port, sa_family_t family = AF_INET);
    ~TcpConnection();

    int getfd() const;

    bool connect();
    ssize_t read_size(size_t* data_size);
    ssize_t write_size(size_t* data_size);
    ssize_t read_from();
    ssize_t write_to();
};

using TC = TcpConnection;

#endif