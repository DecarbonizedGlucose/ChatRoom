#include "../include//CommManager.hpp"
#include "../include/TopClient.hpp"
#include "../include/TcpClient.hpp"
#include "../../global/include/threadpool.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/logging.hpp"
#include "../include/sqlite.hpp"
#include "../../global/include/safe_queue.hpp"

CommManager::CommManager(TopClient* client)
    : top_client(client) {
    clients[0] = top_client->message_client;
    clients[1] = top_client->command_client;
    clients[2] = top_client->data_client;
    sqlite_con = new SQLiteController("~/.local/share/ChatRoom/chat.db");
    // sqlite初始化
    sqlite_con->connect();
}

/* ---------- Pure Input & Output ---------- */

std::string CommManager::read(int idx) {
    std::string proto;
    clients[idx]->socket->receive_protocol(proto);
    return proto;
}

auto CommManager::read_async(int idx) {
    return top_client->pool->submit([this, idx](){
        std::string proto;
        clients[idx]->socket->receive_protocol(proto);
        return proto;
    });
}

void CommManager::send(int idx, const std::string& proto) {
    log_debug("Sending data to connection {} of size {}", idx, proto.size());
    bool success = clients[idx]->socket->send_protocol(proto);
    if (success) {
        log_debug("Successfully sent data to connection {}", idx);
    } else {
        log_error("Failed to send data to connection {}", idx);
    }
}

auto CommManager::send_async(int idx, const std::string& proto) {
    return top_client->pool->submit([this, idx, proto](){
        return clients[idx]->socket->send_protocol(proto);
    });
}

std::string CommManager::read_nb(int idx) {
    std::string proto;
    DataSocket::RecvState state;
    while (*cont) {
        state = clients[idx]->socket->receive_protocol_with_state(proto);
        switch (state) {
            case DataSocket::RecvState::Success: return proto;
            case DataSocket::RecvState::NoMoreData: continue;
            case DataSocket::RecvState::Disconnected: {
                log_info("Connection {} disconnected", idx);
                return "";
            }
            default: {
                log_error("Error receiving data from connection {}: {}", idx, proto);
                return "";
            }
        }
    }
    return "";
}

void CommManager::send_nb(int idx, const std::string& proto) {
    while (*cont) {
        if (clients[idx]->socket->send_protocol(proto)) {
            return;
        }
    }
    return;
}

/* ---------- Handlers ---------- */

// message

ChatMessage CommManager::handle_receive_message(bool nb) {
    std::string proto;
    if (!nb) proto = this->read(0);
    else proto = this->read_nb(0);
    return get_chat_message(proto);
}

void CommManager::handle_send_message(const std::string& sender, const std::string& receiver,
                                      bool is_group_msg, const std::string& text,
                                      bool pin, const std::string& file_name,
                                      std::size_t file_size, const std::string& file_hash,
                                      bool nb) {
    std::string env_out = create_message_string(sender, receiver, is_group_msg, text,
                                             pin, file_name, file_size, file_hash);
    if (!nb) this->send(0, env_out);
    else this->send_nb(0, env_out);
}

void CommManager::handle_manage_message(const ChatMessage& msg) {
    // 存到本地 (SQLite)
    // 根据打开的聊天窗口，决定是否需要显示
}

// command

CommandRequest CommManager::handle_receive_command(bool nb) {
    std::string proto;
    if (!nb) proto = this->read(1);
    else proto = this->read_nb(1);
    return get_command_request(proto);
}

void CommManager::handle_send_command(Action action, const std::string& sender,
                                      std::initializer_list<std::string> args,
                                      bool nb) {
    log_debug("Preparing to send command: action={}, sender={}", static_cast<int>(action), sender);
    std::string env_out = create_command_string(action, sender, args);
    log_debug("Created command string of size {}", env_out.size());
    if (!nb) this->send(1, env_out);
    else this->send_nb(1, env_out);
    log_debug("Command sent");
}

// void CommManager::handle_save_notify(const CommandRequest& cmd) {
//     //cache.notices.push(description);
// }

// void CommManager::handle_show_notify_exist(const CommandRequest& cmd) {
//     //cache.notices.push("已找到用户: " + user_ID);
// }

// void CommManager::handle_show_notify_not_exist(const CommandRequest& cmd) {
//     //cache.notices.push("用户" + user_ID + " 不存在");
// }

// void CommManager::handle_save_request(const CommandRequest& cmd) {
//     //cache.requests.push(description);
// }

// file chunk

// sync item

// offline messages

// others

void CommManager::handle_send_id() {
    for (int i=0; i<3; ++i) {
        auto env_out = create_command_string(
            Action::Remember_Connection, cache.user_ID, {std::to_string(i)});
        this->send(i, env_out);
    }
    log_info("Sent user ID to all servers");
}

void CommManager::store_relation_network_data(const json& relation_data) {
    // 解析好友数据
    if (relation_data.contains("friends") && relation_data["friends"].is_array()) {
        for (const auto& friend_info : relation_data["friends"]) {
            std::string friend_id = friend_info["id"];
            bool is_blocked = friend_info["blocked"];

            // 缓存好友信息到本地数据库
            sqlite_con->cache_friend(cache.user_ID, friend_id, is_blocked);
            log_debug("Cached friend: {} (blocked: {})", friend_id, is_blocked);

            // 加载到内存
            cache.friend_list[friend_id].blocked = is_blocked;
        }
    }

    // 解析群组数据
    if (relation_data.contains("groups") && relation_data["groups"].is_array()) {
        for (const auto& group_info : relation_data["groups"]) {
            std::string group_id = group_info["id"];
            std::string group_name = group_info["name"];
            std::string owner_id = group_info["owner"];

            // 缓存群组信息到本地数据库
            sqlite_con->cache_group(group_id, group_name, owner_id);
            log_debug("Cached group: {} ({})", group_id, group_name);

            // 加载到内存
            cache.group_list[group_id] = {
                group_name,
                owner_id,
                static_cast<int>(group_info["members"].size()),
                false
            };

            // 解析群成员数据
            if (group_info.contains("members") && group_info["members"].is_array()) {
                for (const auto& member_info : group_info["members"]) {
                    std::string member_id = member_info["id"];
                    bool is_admin = member_info["is_admin"];

                    if (member_id == cache.user_ID) {
                        // 如果是当前用户，更新管理员状态
                        cache.group_list[group_id].is_user_admin = is_admin;
                    }

                    // 缓存群成员信息到本地数据库
                    sqlite_con->cache_group_member(group_id, member_id, is_admin);
                    log_debug("Cached group member: {} in {} (admin: {})",
                             member_id, group_id, is_admin);
                }
            }
        }
    }
}

void CommManager::handle_get_relation_net() {
    std::string env_str = this->read(2);
    auto sync = get_sync_item(env_str);
    if (sync.type() != SyncItem::RELATION_NET_FULL) {
        log_error("Unexpected sync type: {}", static_cast<int>(sync.type()));
        return;
    }
    std::string content = sync.content();
    json j = json::parse(content);
    log_info("Received full relation network data");
    // 使用专门的存储函数
    store_relation_network_data(j);
    log_info("Full relation network data synchronized to local database");
}

void CommManager::handle_get_chat_history() {}

void CommManager::handle_add_friend(const std::string& friend_ID) {
    // 检查是否已经是好友
    if (cache.friend_list.find(friend_ID) != cache.friend_list.end()) {
        log_info("Friend {} already exists", friend_ID);
        return;
    }
    // 添加到缓存
    cache.friend_list[friend_ID] = {false, true}; // 默认未屏蔽，在线
    // 存储到SQLite
    sqlite_con->cache_friend(cache.user_ID, friend_ID);
    log_info("Added friend: {}", friend_ID);
}

void CommManager::handle_remove_friend(const std::string& friend_ID) {
    // 从缓存中删除好友
    cache.friend_list.erase(friend_ID);
    // 从数据库中删除好友记录
    sqlite_con->remove_friend_cache(cache.user_ID, friend_ID);
    log_info("Removed friend: {}", friend_ID);
}

void CommManager::handle_get_friend_status() {
    std::string env_str = this->read(2);
    auto sync = get_sync_item(env_str);
    if (sync.type() != SyncItem::ALL_FRIEND_STATUS) {
        log_error("Unexpected sync type: {}", static_cast<int>(sync.type()));
        return;
    }
    std::string content = sync.content();
    json j = json::parse(content);
    for (auto& item : j) {
        // JSON数组中每个元素是 [friend_id, online_status]
        if (item.is_array() && item.size() == 2) {
            std::string friend_id = item[0];
            bool online_status = item[1];
            // 更新好友在线状态
            if (cache.friend_list.find(friend_id) != cache.friend_list.end()) {
                cache.friend_list[friend_id].online = online_status;
                log_debug("Updated friend {} online status: {}", friend_id, online_status);
            } else {
                log_error("Received status for unknown friend: {}", friend_id);
            }
        } else {
            log_error("Invalid friend status format in JSON array");
        }
    }
    log_info("Updated online status for {} friends", j.size());
}
/* ---------- Print ---------- */

void CommManager::print_friends() {
    int max_length = 0;
    for (const auto& [id, info] : cache.friend_list) {
        max_length = std::max(max_length, static_cast<int>(id.size()));
    }
    std::cout << "好友列表(" << cache.friend_list.size()
              << "): " << std::endl;
    std::cout << std::left << std::setw(max_length) << "Friend ID"
              << " | Status" << std::endl;
    for (const auto& [id, info] : cache.friend_list) {
        std::cout << std::left << std::setw(max_length) << id
                  << " | " << (info.blocked ? "Blocked" : "Active")
                  << " | " << (info.online ? "Online" : "Offline") << std::endl;
    }
}

void CommManager::print_groups() {
    int max_length = 0;
    for (const auto& [id, info] : cache.group_list) {
        max_length = std::max(max_length,
            static_cast<int>(id.size()) + 2 +
            (info.member_count == 0 ? 1
                : (int)log10(abs(info.member_count)) + 1));
    }
    std::cout << "群组列表(" << cache.group_list.size()
              << "): " << std::endl;
    std::cout << std::left << std::setw(max_length) << "Group Name (Members)"
              << " | Group ID" << std::endl;
    for (const auto& [id, info] : cache.group_list) {
        std::cout << std::left << std::setw(max_length)
                  << info.group_name << '('
                  << info.member_count << ')'
                  << " | " << id << std::endl;
    }
}