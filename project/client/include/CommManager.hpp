#pragma once
#include <string>
#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/safe_queue.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include "../../global/abstract/datatypes.hpp"
using json = nlohmann::json;

class TopClient;
class TcpClient;
class SQLiteController;

class ContactCache {
public:
    // 用户
    std::string user_ID;
    std::string user_email;
    std::string user_password_hash; // 存储密码哈希值

    // 好友列表
    struct FriendInfo {
        bool blocked = false; // 是否被屏蔽
        bool online = false;  // 是否在线
    };
    std::unordered_map<std::string, FriendInfo> friend_list;

    // 群组列表
    struct GroupInfo {
        std::string group_name;
        std::string owner_ID;
        int member_count = 0;
        bool is_user_admin = false; // 当前用户是否为管理员
    };
    std::unordered_map<std::string, GroupInfo> group_list;

    // 通知队列
    safe_queue<CommandRequest> notices;
    safe_queue<CommandRequest> real_time_notices; // 实时通知

    // 请求队列
    safe_queue<CommandRequest> requests;
};

class CommManager {
public:
    bool* cont = nullptr;
    SQLiteController* sqlite_con = nullptr;

    CommManager(TopClient* client);

    TopClient* top_client;
    TcpClient* clients[3];

    ContactCache cache;

/* ---------- Pure Input & Output ---------- */
    std::string read(int idx);
    auto read_async(int idx);
    void send(int idx, const std::string& proto);
    auto send_async(int idx, const std::string& proto);
    std::string read_nb(int idx);
    void send_nb(int idx, const std::string& proto);

/* ---------- Handlers ---------- */
    // message
    ChatMessage handle_receive_message(bool nb = true);
    void handle_send_message(const std::string& sender, const std::string& receiver,
                             bool is_group_msg, const std::string& text,
                             bool pin = false, const std::string& file_name = "",
                             std::size_t file_size = 0, const std::string& file_hash = "",
                             bool nb = true);
    void handle_manage_message(const ChatMessage& msg);

    // command
    CommandRequest handle_receive_command(bool nb = true);
    void handle_send_command(Action action, const std::string& sender, std::initializer_list<std::string> args, bool nb = true);

    // void handle_save_notify(const CommandRequest& cmd);
    // void handle_show_notify_exist(const CommandRequest& cmd);
    // void handle_show_notify_not_exist(const CommandRequest& cmd);
    // void handle_save_request(const CommandRequest& cmd);

    // others
    void handle_get_relation_net(); // 不发请求，主动拉取，不知道发来什么
    void handle_send_id();
    void handle_get_chat_history();
    void handle_add_friend(const std::string& friend_ID);
    void handle_remove_friend(const std::string& friend_ID);
    void handle_get_friend_status();
    void handle_join_group(
        const std::string& user_ID,
        const std::string& group_ID
    );
/* ---------- Print ---------- */
    void print_friends();
    void print_groups();


private:
    // 扔进去全存
    void store_relation_network_data(const json& relation_data);
};