#include "include/connection_manager.hpp"
#include "include/dispatcher.hpp"
#include <chrono>

void ConnectionManager::add_conn(TcpServerConnection* conn, int server_index) {
    // 记录连接的时间戳
    user_connections[conn->user_ID][server_index] = conn;
}

void ConnectionManager::remove_user(std::string user_ID) {
    auto it = user_connections.find(user_ID);
    if (it == user_connections.end()) {
        return; // 已经寄了
    }
    for (auto p : it->second) {
        delete p;
    }
    user_connections.erase(it);
    // 还有Redis操作在另外的地方执行了
}

bool ConnectionManager::user_exists(std::string user_ID) {
    bool exists = disp->redis_con->get_user_status(user_ID).first;
    if (!exists) {
        // 删掉用户连接
        remove_user(user_ID);
    }
    return exists;
}
