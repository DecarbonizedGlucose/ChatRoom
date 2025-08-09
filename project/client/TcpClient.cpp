#include "include/TcpClient.hpp"

namespace set_addr_c {
    Addr client_addr[3];
}

TcpClient::TcpClient(std::string server_ip, uint16_t server_port) {
    socket = std::make_shared<ConnectSocket>(server_ip, server_port);
}

void TcpClient::start() {
    socket->connect();
}

void TcpClient::stop() {
    socket->disconnect();
}