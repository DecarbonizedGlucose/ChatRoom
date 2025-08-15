#pragma once

#include "../../io/include/Socket.hpp"
#include "../../io/include/reactor.hpp"
#include <string>
#include <array>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include "TcpServerConnection.hpp"

class Dispatcher;

class ConnectionManager {
private:
    // 掌握生杀大权
    std::unordered_map<std::string, std::array<TcpServerConnection*, 3>> user_connections;
    Dispatcher* disp;

    // 心跳监控相关
    std::thread heartbeat_thread;
    std::mutex user_mutex;
    bool heartbeat_running = false;

    void start_heartbeat_monitor();
    void check_user_timeouts();
    void send_heartbeat_to_user(const std::string& user_ID);

public:
    ConnectionManager(Dispatcher* disp) : disp(disp) {
        start_heartbeat_monitor();
    }

    ~ConnectionManager() {
        heartbeat_running = false;
        if (heartbeat_thread.joinable()) {
            heartbeat_thread.join();
        }
    }

    void add_temp_conn(TcpServerConnection* conn, int server_index);
    void add_conn(TcpServerConnection* conn, int server_index);
    void remove_user(std::string user_ID);
    void destroy_connection(std::string user_ID);
    bool user_exists(std::string user_ID);
    TcpServerConnection* get_connection(const std::string& user_ID, int server_index = 0);
    void update_user_activity(const std::string& user_ID);
};
