#include "../include//CommManager.hpp"
#include "../include/TopClient.hpp"
#include "../include/TcpClient.hpp"
#include "../../global/include/threadpool.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/logging.hpp"
#include "../include/sqlite.hpp"
#include "../include/cfile_manager.hpp"
#include <ctime>
#include <fstream>
#include <algorithm>

CommManager::CommManager(TopClient* client)
    : top_client(client) {
    clients[0] = top_client->message_client;
    clients[1] = top_client->command_client;
    clients[2] = top_client->data_client;
    sqlite_con = new SQLiteController("~/.local/share/ChatRoom/chat.db");
    // sqlite初始化
    sqlite_con->connect();
    file_manager = new CFileManager(top_client->pool, this);
}

/* ---------- Pure Input & Output ---------- */

std::string CommManager::read(int idx) {
    std::string proto;
    clients[idx]->socket->receive_protocol(proto);
    log_debug("Received data from connection {}: size={}", idx, proto.size());
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
    auto time = std::time(nullptr);
    std::string env_out = create_message_string(sender, receiver, is_group_msg, time, text,
                                                pin, file_name, file_size, file_hash);
    if (!nb) this->send(0, env_out);
    else this->send_nb(0, env_out);

    // 存到本地
    sqlite_con->cache_chat_message(
        sender, receiver, is_group_msg, time, text, pin, file_name, file_size, file_hash
    );

    auto msg = create_chat_message(sender, receiver, is_group_msg, time, text,
                                    pin, file_name, file_size, file_hash);
    cache.messages[receiver].push_back(msg);

    // 更新会话
    // 确定会话ID
    auto& conv = cache.conversations[receiver];
    if (time > conv.last_time) {
        conv.last_time = time;
        conv.last_message = is_group_msg ? sender + ": " : "";
        if (text.empty() && !file_name.empty()) {
            conv.last_message += "[文件] " + file_name;
        }
        else {
            conv.last_message += text;
        }
        // 更新会话排序
        cache.update_conversation_order(receiver);
    }
}

void CommManager::handle_manage_message(const ChatMessage& msg) {
    // 存到本地 (SQLite)
    sqlite_con->cache_chat_message(
        msg.sender(), msg.receiver(), msg.is_group(),
        msg.timestamp(), msg.text(), msg.pin(),
        msg.payload().file_name(),
        msg.payload().file_size(),
        msg.payload().file_hash()
    );
    // 确定会话ID（群聊用receiver, 私聊用sender）
    std::string conv_id = msg.is_group() ? msg.receiver() : msg.sender();
    cache.messages[conv_id].push_back(msg);
    cache.new_messages.push(msg);

    // 跳过空的会话ID
    if (conv_id.empty()) {
        log_error("Empty conversation_id in handle_manage_message, skipping");
        return;
    }

    auto& conv = cache.conversations[conv_id];

    // 如果会话还没有正确的ID和名称, 设置它们
    if (conv.id.empty()) {
        conv.id = conv_id;
        conv.name = conv_id;
        conv.is_group = msg.is_group();
    }

    if (msg.timestamp() > conv.last_time) {
        conv.last_time = msg.timestamp();
        if (msg.is_group()) {
            conv.last_message = msg.sender() + ": ";
        }
        if (msg.text().empty() && !msg.payload().file_name().empty()) {
            conv.last_message += "[文件] " + msg.payload().file_name();
        }
        else {
            conv.last_message += msg.text();
        }

        // 更新会话排序
        cache.update_conversation_order(conv_id);
    }
}

// command

CommandRequest CommManager::handle_receive_command(bool nb) {
    std::string proto;
    if (!nb) {
        log_debug("About to read command from connection 1...");
        proto = this->read(1);
        log_debug("Received command proto of size: {}", proto.size());
    } else {
        proto = this->read_nb(1);
    }

    try {
        auto cmd = get_command_request(proto);
        log_debug("Successfully parsed command: action={}", cmd.action());
        return cmd;
    } catch (const std::exception& e) {
        log_error("Failed to parse command: {}", e.what());
        // 打印原始数据的前几个字节用于调试
        if (proto.size() > 0) {
            log_error("Proto data (first 20 bytes): {}",
                     proto.substr(0, std::min((size_t)20, proto.size())));
        }
        throw;
    }
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

// file chunk

FileChunk CommManager::handle_receive_file_chunk(bool nb) {
    std::string proto;
    if (!nb) proto = this->read(2);
    else proto = this->read_nb(2);
    return get_file_chunk(proto);
}

void CommManager::handle_send_file_chunk(
    const std::string& file_id,
    const std::vector<char>& data,
    int chunk_index,
    int total_chunks,
    bool is_last_chunk,
    bool nb
) {
    log_debug("Preparing to send file chunk: file_id={}, chunk_index={}, total_chunks={}, is_last_chunk={}",
              file_id, chunk_index, total_chunks, is_last_chunk);

    auto env_out = create_file_chunk_string(file_id, data, chunk_index, total_chunks, is_last_chunk);
    log_debug("Created file chunk string of size {}", env_out.size());

    if (!nb) this->send(2, env_out);
    else this->send_nb(2, env_out);

    log_debug("File chunk sent");
}

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
    log_debug("Received relation network data: {}", relation_data.dump(2));

    // 解析好友数据
    if (relation_data.contains("friends") && relation_data["friends"].is_array()) {
        for (const auto& friend_info : relation_data["friends"]) {
            std::string friend_id = friend_info["id"];
            bool is_blocked = friend_info["blocked"];

            log_debug("Processing friend: {} (blocked: {})", friend_id, is_blocked);

            // 检查是否是自己, 如果是则跳过
            if (friend_id == cache.user_ID) {
                log_error("Server sent user's own ID {} in friends list - skipping", friend_id);
                continue;
            }

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
                        // 如果是当前用户, 更新管理员状态
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

void CommManager::handle_get_chat_history() {
    log_debug("handle_get_chat_history called");

    try {
        // 从数据连接（连接2）读取离线消息
        std::string env_str = this->read(2);

        // 解析OfflineMessages
        auto offline_msgs = get_offline_messages(env_str);

        int message_count = offline_msgs.messages_size();
        log_info("Received {} offline messages", message_count);

        if (message_count == 0) {
            log_debug("No offline messages to process");
            return;
        }

        // 处理每个离线消息
        for (int i = 0; i < message_count; ++i) {
            const ChatMessage& msg = offline_msgs.messages(i);

            // 存储到本地数据库并更新缓存
            handle_manage_message(msg);

            log_debug("Processed offline message from {} to {} (group: {}) at timestamp {}",
                     msg.sender(), msg.receiver(), msg.is_group(), msg.timestamp());
        }

        // 更新会话列表（如果有新的会话）
        update_conversation_list();

        log_info("Successfully processed {} offline messages", message_count);

    } catch (const std::exception& e) {
        log_error("Error handling offline messages: {}", e.what());
    }
}

void CommManager::handle_add_friend(const std::string& friend_ID) {
    // 检查是否试图添加自己为好友
    if (friend_ID == cache.user_ID) {
        log_info("Cannot add self {} as friend", friend_ID);
        return;
    }

    // 检查是否已经是好友
    if (cache.friend_list.find(friend_ID) != cache.friend_list.end()) {
        log_info("Friend {} already exists", friend_ID);
        return;
    }
    // 添加到缓存
    cache.friend_list[friend_ID] = {false, true}; // 默认未屏蔽, 在线

    // 直接存储到SQLite（SQLiteController内部有mutex保护）
    if (sqlite_con->cache_friend(cache.user_ID, friend_ID)) {
        log_info("Added friend to database: {}", friend_ID);
    } else {
        log_error("Failed to add friend to database: {}", friend_ID);
    }

    log_info("Added friend to cache: {}", friend_ID);
}

void CommManager::handle_remove_friend(const std::string& friend_ID) {
    // 从缓存中删除好友
    cache.friend_list.erase(friend_ID);
    // 从数据库中删除好友记录
    sqlite_con->remove_friend_cache(cache.user_ID, friend_ID);
    log_info("Removed friend: {}", friend_ID);
}

void CommManager::handle_block_friend(const std::string& friend_ID) {
    // 标记好友为屏蔽
    if (cache.friend_list.find(friend_ID) != cache.friend_list.end()) {
        cache.friend_list[friend_ID].blocked = true;
        log_info("Blocked friend: {}", friend_ID);
    }
    // sqlite
    sqlite_con->update_friend_block_status(cache.user_ID, friend_ID, true);
}

void CommManager::handle_unblock_friend(const std::string& friend_ID) {
    // 标记好友为未屏蔽
    if (cache.friend_list.find(friend_ID) != cache.friend_list.end()) {
        cache.friend_list[friend_ID].blocked = false;
        log_info("Unblocked friend: {}", friend_ID);
    }
    // sqlite
    sqlite_con->update_friend_block_status(cache.user_ID, friend_ID, false);
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

void CommManager::handle_create_group(const std::string& group_ID, const std::string& group_name) {

}

void CommManager::handle_join_group(const std::string& group_ID) {

}

void CommManager::handle_leave_group(const std::string& group_ID) {
    // 从缓存中删除群组
    cache.group_list.erase(group_ID);
    // 从数据库中删除群组记录
    sqlite_con->remove_group_cache(group_ID);
    log_info("Left group: {}", group_ID);
    // 还需要更新会话列表
}

void CommManager::handle_add_admin(const std::string& group_ID, const std::string& user_ID) {}

void CommManager::handle_remove_admin(const std::string& group_ID, const std::string& user_ID) {}

void CommManager::handle_reply_heartbeat() {
    log_debug("Received heartbeat from server, sending response");
    // 立即回复心跳
    handle_send_command(Action::HEARTBEAT, cache.user_ID, {});
}

/* ---------- Print ---------- */

void CommManager::print_friends() {
    log_debug("Current user_ID: {}", cache.user_ID);
    log_debug("Friend list contains {} entries:", cache.friend_list.size());
    for (const auto& [id, info] : cache.friend_list) {
        log_debug("  - Friend ID: {} (blocked: {}, online: {})", id, info.blocked, info.online);
    }

    int max_length = 0;
    for (const auto& [id, info] : cache.friend_list) {
        max_length = std::max(max_length, static_cast<int>(id.size()));
    }
    max_length = std::max(max_length, 9);
    std::cout << "好友列表(" << cache.friend_list.size()
              << "): " << std::endl;
    std::cout << std::left << std::setw(max_length) << "Friend ID"
              << " | Blocked?" << " | Status" << std::endl;
    for (const auto& [id, info] : cache.friend_list) {
        std::cout << std::left << std::setw(max_length) << id
                  << " | " << (info.blocked ? "Yes     " : "No      ")
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
    max_length = std::max(max_length, 20);
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

/* ---------- 会话管理 ---------- */

void CommManager::update_conversation_list() {
    log_debug("Updating conversation list with {} friends and {} groups",
              cache.friend_list.size(), cache.group_list.size());

    // 根据好友列表更新会话
    for (const auto& [friend_id, friend_info] : cache.friend_list) {
        log_debug("Processing friend: '{}'", friend_id);

        if (friend_id.empty()) {
            log_error("Found empty friend_id, skipping");
            continue;
        }

        if (cache.conversations.find(friend_id) == cache.conversations.end()) {
            // 创建新会话
            ContactCache::ConversationInfo conv_info;
            conv_info.id = friend_id;
            conv_info.name = friend_id;
            conv_info.is_group = false;
            conv_info.last_time = 0;

            // 安全地获取最后一条消息
            if (!cache.messages[friend_id].empty()) {
                auto last_msg = cache.messages[friend_id].back().value();
                auto text = last_msg.text();
                if (text.empty()) {
                    conv_info.last_message = "[文件] " + last_msg.payload().file_name();
                } else {
                    conv_info.last_message = text;
                }
                conv_info.last_time = last_msg.timestamp();
            } else {
                conv_info.last_message = "暂无消息";
            }
            conv_info.unread_count = 0;

            cache.conversations[friend_id] = conv_info;

            // 新会话添加到排序列表末尾（没有消息的会话排在后面）
            cache.sorted_conversation_list.push_back(friend_id);

            log_debug("Created conversation for friend: '{}'", friend_id);
        }
    }    // 根据群组列表更新会话
    for (const auto& [group_id, group_info] : cache.group_list) {
        if (cache.conversations.find(group_id) == cache.conversations.end()) {
            // 创建新会话
            ContactCache::ConversationInfo conv_info;
            conv_info.id = group_id;
            conv_info.name = group_info.group_name;
            conv_info.is_group = true;
            conv_info.last_time = 0;

            // 安全地获取最后一条消息
            if (!cache.messages[group_id].empty()) {
                auto last_msg = cache.messages[group_id].back().value();
                conv_info.last_message = last_msg.sender() + ": ";
                auto text = last_msg.text();
                if (text.empty()) {
                    conv_info.last_message += "[文件] " + last_msg.payload().file_name();
                } else {
                    conv_info.last_message += text;
                }
                conv_info.last_time = last_msg.timestamp();
            } else {
                conv_info.last_message = "暂无消息";
            }
            conv_info.unread_count = 0;

            cache.conversations[group_id] = conv_info;

            // 新会话添加到排序列表末尾（没有消息的会话排在后面）
            cache.sorted_conversation_list.push_back(group_id);
        }
    }
}

void CommManager::load_conversation_history() {
    // 从SQLite加载聊天历史
    for (auto friend_info : cache.friend_list) {
        auto chat_history = sqlite_con->get_private_chat_history(cache.user_ID, friend_info.first);
        for (const auto& row : chat_history) {
            if (row.size() >= 8) {  // 从数据库表拿了8个字段
                ChatMessage msg;
                if (row[4] == "1") {
                    msg = create_chat_message(
                        row[0], row[1], false, std::stoll(row[2]),
                        row[3], true, row[5], std::stoll(row[6]), row[7]
                    );
                } else {
                    msg = create_chat_message(
                        row[0], row[1], false, std::stoll(row[2]),
                        row[3], false
                    );
                }
                cache.messages[friend_info.first].push_front(msg);
            }
        }
    }
    for (auto group_info : cache.group_list) {
        auto chat_history = sqlite_con->get_group_chat_history(group_info.first);
        for (const auto& row : chat_history) {
            if (row.size() >= 8) {  // 从数据库表拿了8个字段
                ChatMessage msg;
                if (row[6] == "1") {
                    msg = create_chat_message(
                        row[0], row[1], true, std::stoll(row[2]),
                        row[3], true, row[5], std::stoll(row[6]), row[7]
                    );
                } else {
                    msg = create_chat_message(
                        row[0], row[1], true, std::stoll(row[2]),
                        row[3], false
                    );
                }
                cache.messages[group_info.first].push_front(msg);
            }
        }
    }
}

void CommManager::send_text_message(const std::string& receiver_id, bool is_group, const std::string& text) {
    // 发送文本消息 - 使用阻塞发送确保消息被发送
    handle_send_message(
        cache.user_ID,          // sender
        receiver_id,            // receiver
        is_group,               // is_group_msg
        text,                   // text
        false,                  // pin (not a file)
        "",                     // file_name
        0,                      // file_size
        "",                     // file_hash
        false                   // nb (blocking - 确保消息被发送)
    );

    log_info("Sent text message to {}: {}", receiver_id, text);

    // 更新会话信息
    if (cache.conversations.find(receiver_id) != cache.conversations.end()) {
        cache.conversations[receiver_id].last_message = text;
        cache.conversations[receiver_id].last_time = std::time(nullptr);
        // 更新会话排序
        cache.update_conversation_order(receiver_id);
    }
}

void CommManager::send_file_message(
    const std::string& receiver_id,
    bool is_group,
    const ClientFilePtr& file,
    const std::string& file_id
) {
    // 发送文件消息
    handle_send_message(
        cache.user_ID,          // sender
        receiver_id,            // receiver
        is_group,               // is_group_msg
        "",                     // text (empty for file)
        true,                   // pin (file message)
        file->file_name + "@" + file_id, // 丑陋的解决方案
        file->file_size,        // file_size
        file->file_hash,        // file_hash
        true                    // nb (non-blocking)
    );

    log_info("Sent file message to {}: {}", receiver_id, file->file_name);

    // 更新会话信息
    if (cache.conversations.find(receiver_id) != cache.conversations.end()) {
        cache.conversations[receiver_id].last_message = "[文件] " + file->file_name;
        cache.conversations[receiver_id].last_time = std::time(nullptr);
        // 更新会话排序
        cache.update_conversation_order(receiver_id);
    }
}

void CommManager::send_text_with_file(
        const std::string& receiver_id,
        bool is_group,
        const std::string& text,
        const std::string& file_path) {

}

std::vector<ChatMessage> CommManager::get_conversation_messages(const std::string& conversation_id, int limit) {
    auto messages = cache.messages[conversation_id].copy_max_num(limit);
    std::sort(messages.begin(), messages.end(), [](const ChatMessage& a, const ChatMessage& b) {
        return a.timestamp() < b.timestamp();
    });
    if ((int)messages.size() > limit) {
        messages.erase(messages.begin(), messages.end() - limit);
    }
    return messages;
}

void ContactCache::update_conversation_order(const std::string& conversation_id) {
    // 简单直接：从列表中移除该会话（如果存在）
    auto it = std::find(sorted_conversation_list.begin(), sorted_conversation_list.end(), conversation_id);
    if (it != sorted_conversation_list.end()) {
        sorted_conversation_list.erase(it);
    }

    // 将该会话添加到列表开头（最新的在前面）
    sorted_conversation_list.insert(sorted_conversation_list.begin(), conversation_id);
}

auto ContactCache::get_sorted_conversations()
    -> std::vector<std::pair<std::string, ContactCache::ConversationInfo*>> {
    std::vector<std::pair<std::string, ConversationInfo*>> result;

    // 使用排序列表作为主要排序依据
    for (const auto& conv_id : sorted_conversation_list) {
        // 跳过空的会话ID
        if (conv_id.empty()) {
            continue;
        }

        auto it = conversations.find(conv_id);
        if (it != conversations.end()) {
            result.push_back({conv_id, &it->second});
        }
    }

    // 添加不在排序列表中的新会话（一般不应该发生, 但作为安全机制）
    for (auto& [id, info] : conversations) {
        // 跳过空的会话ID
        if (id.empty()) {
            continue;
        }

        if (std::find(sorted_conversation_list.begin(), sorted_conversation_list.end(), id) == sorted_conversation_list.end()) {
            result.push_back({id, &info});
            // 同时更新排序列表
            sorted_conversation_list.push_back(id);
        }
    }

    return result;
}