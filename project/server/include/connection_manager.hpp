#pragma once

#include "../../io/include/Socket.hpp"
#include "../../io/include/reactor.hpp"
#include <string>
#include <array>
#include <unordered_map>
#include "TcpServerConnection.hpp"

class Dispatcher;

class ConnectionManager {
private:
    // 掌握生杀大权
    std::unordered_map<std::string, std::array<TcpServerConnection*, 3>> user_connections;
    Dispatcher* disp;

public:
    ConnectionManager(Dispatcher* disp) : disp(disp) {}

    void add_conn(TcpServerConnection* conn, int server_index);
    void remove_user(std::string user_ID);
    bool user_exists(std::string user_ID);
};
