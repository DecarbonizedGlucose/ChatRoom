#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include "../../io/include/Socket.hpp"
#include <string>
#include <memory>
#include <utility>

namespace set_addr {
    using Addr = std::pair<std::string, uint16_t>;
    Addr client_addr[3];

    bool fetch_addr() {}
}

class TcpClient {
private:
    CSocketPtr socket; // 直接拥有socket

public:
    TcpClient(std::string server_ip, uint16_t server_port) {
        socket = std::make_shared<ConnectSocket>(server_ip, server_port);
    }

    void start() {
        socket->connect();
    }

    void stop() {
        socket->disconnect();
    }
};

using TcpClientPtr = std::shared_ptr<TcpClient>;

#endif // TCP_CLIENT_HPP