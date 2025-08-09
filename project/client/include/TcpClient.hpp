#pragma once

#include "../../io/include/Socket.hpp"
#include "../../global/include/safe_queue.hpp"
#include <string>
#include <memory>
#include <utility>

namespace set_addr_c {
    using Addr = std::pair<std::string, uint16_t>;
    extern Addr client_addr[3];
}

class TcpClient {
public:
    CSocketPtr socket;
    safe_queue<std::string> send_queue;

    TcpClient(std::string server_ip, uint16_t server_port);

    void start();

    void stop();
};
