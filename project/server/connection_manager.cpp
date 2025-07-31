#include "include/connection_manager.hpp"
#include "include/dispatcher.hpp"
#include "../../global/include/logging.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../include/handler.hpp"
#include "../global/include/time_utils.hpp"

void ConnectionManager::add_conn(TcpServerConnection* conn, int server_index) {
    std::lock_guard<std::mutex> lock(user_mutex);
    // 记录连接的时间戳
    user_connections[conn->user_ID][server_index] = conn;
    disp->redis_con->set_user_status(conn->user_ID, true);
    log_debug("Added connection {} for user: {}", server_index, conn->user_ID);
}

void ConnectionManager::remove_user(std::string user_ID) {
    std::lock_guard<std::mutex> lock(user_mutex);
    auto it = user_connections.find(user_ID);
    if (it == user_connections.end()) {
        return; // 已经寄了
    }
    for (auto p : it->second) {
        delete p;
    }
    user_connections.erase(it);
    disp->redis_con->set_user_status(user_ID, false);
    disp->mysql_con->update_user_status(user_ID, false); // 更新MySQL状态
    log_debug("Removed user: {}", user_ID);
}

bool ConnectionManager::user_exists(std::string user_ID) {
    bool exists = disp->redis_con->get_user_status(user_ID).first;
    if (!exists) {
        // 删掉用户连接
        remove_user(user_ID);
    }
    return exists;
}

TcpServerConnection* ConnectionManager::get_connection(const std::string& user_ID, int server_index) {
    std::lock_guard<std::mutex> lock(user_mutex);
    auto it = user_connections.find(user_ID);
    if (it != user_connections.end() && it->second[server_index]) {
        return it->second[server_index];
    }
    return nullptr; // 没有找到连接
}

void ConnectionManager::update_user_activity(const std::string& user_ID) {
    // 更新用户状态，包含当前时间戳
    disp->redis_con->set_user_status(user_ID, true);
    disp->mysql_con->update_user_last_active(user_ID);
}

void ConnectionManager::start_heartbeat_monitor() {
    heartbeat_running = true;
    heartbeat_thread = std::thread([this]() {
        while (heartbeat_running) {
            std::this_thread::sleep_for(std::chrono::seconds(30)); // 每30秒检查一次
            if (heartbeat_running) {
                check_user_timeouts();
            }
        }
    });
    log_info("Heartbeat monitor thread started");
}

void ConnectionManager::check_user_timeouts() {
    std::vector<std::string> all_users;

    // 快速获取用户列表，减少锁持有时间
    {
        std::lock_guard<std::mutex> lock(user_mutex);
        for (const auto& [user_ID, connections] : user_connections) {
            all_users.push_back(user_ID);
        }
    }

    std::vector<std::string> users_to_remove;

    // 在锁外进行Redis检查
    for (const auto& user_ID : all_users) {
        // 检查用户状态，如果last_active时间过长则移除
        auto status = disp->redis_con->get_user_status(user_ID);
        bool is_online = status.first;
        std::time_t last_active = status.second;

        if (is_online) {
            auto now = std::time(nullptr);
            auto inactive_time = now - last_active;

            if (inactive_time > 90) { // 90秒超时
                users_to_remove.push_back(user_ID);
                log_info("User {} timed out ({}s inactive), removing connection", user_ID, inactive_time);
            } else if (inactive_time > 30) { // 30秒发送心跳
                send_heartbeat_to_user(user_ID);
            }
        }
    }

    // 移除超时用户
    for (const auto& user_ID : users_to_remove) {
        disp->redis_con->set_user_status(user_ID, false);
        remove_user(user_ID);
    }
}

void ConnectionManager::send_heartbeat_to_user(const std::string& user_ID) {
    auto cmd_conn = get_connection(user_ID, 1); // 使用命令连接发送心跳
    if (cmd_conn) {
        std::string heartbeat_cmd = create_command_string(Action::HEARTBEAT, "", {});
        try_send(
            this,
            cmd_conn,
            heartbeat_cmd
        );
        log_debug("Sent heartbeat to user: {}", user_ID);
    } else {
        log_error("Failed to send heartbeat to user {} - command connection not found", user_ID);
    }
}
