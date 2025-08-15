#include "include/connection_manager.hpp"
#include "include/dispatcher.hpp"
#include "../../global/include/logging.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../include/handler.hpp"
#include "../global/include/time_utils.hpp"

void ConnectionManager::add_conn(TcpServerConnection* conn, int server_index) {
    if (conn == nullptr) {
        log_error("Attempted to add null connection for server_index: {}", server_index);
        return;
    }

    if (server_index < 0 || server_index >= 3) {
        log_error("Invalid server_index: {} for user: {}", server_index, conn->user_ID);
        return;
    }

    std::lock_guard<std::mutex> lock(user_mutex);

    // 如果该位置已有连接, 先安全清理
    if (user_connections[conn->user_ID][server_index] != nullptr) {
        log_info("Replacing existing connection {} for user: {}", server_index, conn->user_ID);
        delete user_connections[conn->user_ID][server_index];
    }

    // 添加新连接
    user_connections[conn->user_ID][server_index] = conn;

    try {
        disp->redis_con->set_user_status(conn->user_ID, true);
        log_info("Added connection {} for user: {}", server_index, conn->user_ID);
    } catch (const std::exception& e) {
        log_error("Error updating user {} status during connection add: {}", conn->user_ID, e.what());
    }
}

void ConnectionManager::add_temp_conn(TcpServerConnection* conn, int server_index) {
    if (conn == nullptr) {
        log_error("Attempted to add null connection for server_index: {}", server_index);
        return;
    }

    if (server_index < 0 || server_index >= 3) {
        log_error("Invalid server_index: {} for user: {}", server_index, conn->temp_user_ID);
        return;
    }

    std::lock_guard<std::mutex> lock(user_mutex);

    // 如果该位置已有连接, 先安全清理
    if (user_connections[conn->temp_user_ID][server_index] != nullptr) {
        log_info("Replacing existing connection {} for user: {}", server_index, conn->temp_user_ID);
        delete user_connections[conn->temp_user_ID][server_index];
    }

    // 添加新连接
    user_connections[conn->temp_user_ID][server_index] = conn;

    try {
        disp->redis_con->set_user_status(conn->temp_user_ID, true);
        log_info("Added connection {} for user: {}", server_index, conn->temp_user_ID);
    } catch (const std::exception& e) {
        log_error("Error updating user {} status during connection add: {}", conn->temp_user_ID, e.what());
    }
}

void ConnectionManager::remove_user(std::string user_ID) {
    std::lock_guard<std::mutex> lock(user_mutex);
    auto it = user_connections.find(user_ID);
    if (it == user_connections.end()) {
        log_debug("User {} already removed or not found", user_ID);
        return; // 用户已经被移除或不存在
    }

    // 从映射表中移除用户
    user_connections.erase(it);

    // 更新用户状态（在锁内进行, 保证一致性）
    try {
        disp->redis_con->set_user_status(user_ID, false);
        if (user_ID[0] != '_') // 不是临时用户名
            disp->mysql_con->update_user_status(user_ID, false);
        else
            disp->redis_con->del_user_status(user_ID);
        log_info("Successfully removed user: {}", user_ID);
    } catch (const std::exception& e) {
        log_error("Error updating user {} status during removal: {}", user_ID, e.what());
    }
}

void ConnectionManager::destroy_connection(std::string user_ID) {
    std::lock_guard<std::mutex> lock(user_mutex);
    auto it = user_connections.find(user_ID);
    if (it != user_connections.end()) {
        for (int i = 0; i < 3; ++i) {
            if (it->second[i] != nullptr) {
                log_debug("Destroying connection {} for user: {}", i, user_ID);
                delete it->second[i];
                it->second[i] = nullptr; // 防止重复删除
            }
        }
        user_connections.erase(it);
        try {
            disp->redis_con->set_user_status(user_ID, false);
            if (user_ID[0] != '_')
                disp->mysql_con->update_user_status(user_ID, false);
            else
                disp->redis_con->del_user_status(user_ID);
            log_info("Successfully removed user: {}", user_ID);
        } catch (const std::exception& e) {
            log_error("Error updating user {} status during removal: {}", user_ID, e.what());
        }
        log_info("Destroyed all connections for user: {}", user_ID);
    } else {
        log_debug("No connections found for user: {}", user_ID);
    }
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
    if (server_index < 0 || server_index >= 3) {
        log_error("Invalid server_index: {} for user: {}", server_index, user_ID);
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(user_mutex);
    auto it = user_connections.find(user_ID);
    if (it != user_connections.end()) {
        TcpServerConnection* conn = it->second[server_index];
        if (conn != nullptr) {
            // 可以在这里添加连接有效性检查
            return conn;
        } else {
            log_debug("Connection {} for user {} is null", server_index, user_ID);
        }
    } else {
        log_debug("User {} not found in connections", user_ID);
    }
    return nullptr; // 没有找到有效连接
}

void ConnectionManager::update_user_activity(const std::string& user_ID) {
    // 更新用户状态, 包含当前时间戳
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

    // 快速获取用户列表, 减少锁持有时间
    {
        std::lock_guard<std::mutex> lock(user_mutex);
        all_users.reserve(user_connections.size()); // 预分配空间
        for (const auto& [user_ID, connections] : user_connections) {
            all_users.push_back(user_ID);
        }
    }

    std::vector<std::string> users_to_destroy;
    users_to_destroy.reserve(all_users.size()); // 预分配空间

    // 在锁外进行Redis检查, 避免长时间持锁
    for (const auto& user_ID : all_users) {
        try {
            // 检查用户状态, 如果last_active时间过长则移除
            auto status = disp->redis_con->get_user_status(user_ID);
            bool is_online = status.first;
            std::int64_t last_active = status.second;

            if (is_online) {
                auto now = now_us();
                auto inactive_time = now - last_active;

                constexpr std::int64_t TIMEOUT_US = 90LL * 1000000;
                constexpr std::int64_t HEARTBEAT_US = 30LL * 1000000;
                if (inactive_time > TIMEOUT_US) { // 90秒超时（微秒）
                    users_to_destroy.push_back(user_ID);
                    log_info("User {} timed out ({}us inactive), marking for removal", user_ID, inactive_time);
                } else if (inactive_time > HEARTBEAT_US) { // 30秒发送心跳（微秒）
                    send_heartbeat_to_user(user_ID);
                }
            } else {
                // 用户已离线, 标记移除
                users_to_destroy.push_back(user_ID);
                log_debug("User {} is offline, marking for removal", user_ID);
            }
        } catch (const std::exception& e) {
            log_error("Error checking status for user {}: {}", user_ID, e.what());
            // 发生错误时也标记移除, 避免僵尸连接
            users_to_destroy.push_back(user_ID);
        }
    }

    // 批量移除超时用户
    for (const auto& user_ID : users_to_destroy) {
        try {
            disp->redis_con->set_user_status(user_ID, false);
            destroy_connection(user_ID);
        } catch (const std::exception& e) {
            log_error("Error removing user {}: {}", user_ID, e.what());
        }
    }

    if (!users_to_destroy.empty()) {
        log_info("Heartbeat check completed: removed {} users", users_to_destroy.size());
    }
}

void ConnectionManager::send_heartbeat_to_user(const std::string& user_ID) {
    try {
        auto cmd_conn = get_connection(user_ID, 1); // 使用命令连接发送心跳
        if (cmd_conn != nullptr) {
            std::string heartbeat_cmd = create_command_string(Action::HEARTBEAT, "", {});
            try_send(this, cmd_conn, heartbeat_cmd);
        } else {
            log_debug("Skipping heartbeat for user {} : connection[1] is nullptr", user_ID);
        }
    } catch (const std::exception& e) {
        log_error("Exception while sending heartbeat to user {}: {}", user_ID, e.what());
    }
}