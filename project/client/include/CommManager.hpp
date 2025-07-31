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

    // 会话管理
    struct ConversationInfo {
        std::string id;              // 会话ID（用户ID或群组ID）
        std::string name;            // 显示名称
        bool is_group = false;       // 是否为群聊
        std::string last_message;    // 最后一条消息
        std::time_t last_time = 0;   // 最后消息时间
        int unread_count = 0;        // 未读消息数
    };
    std::unordered_map<std::string, ConversationInfo> conversations;
    std::string current_conversation_id; // 当前打开的会话ID

    // 消息队列（用于实时接收）
    safe_queue<ChatMessage> messages;

    // 会话排序管理（实时更新的消息列表顺序）
    // 原理：当有新消息时，将对应会话移到列表开头，实现类似QQ/微信的效果
    std::vector<std::string> sorted_conversation_list;

    // 更新会话排序（有新消息时调用，自动将会话置顶）
    void update_conversation_order(const std::string& conversation_id);

    // 获取排序后的会话列表（UI显示用）
    std::vector<std::pair<std::string, ConversationInfo*>> get_sorted_conversations();
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
    void handle_reply_heartbeat();

    // 会话管理
    void update_conversation_list();
    void load_conversation_history(const std::string& conversation_id);
    void send_text_message(
        const std::string& receiver_id,
        bool is_group,
        const std::string& text);
    void send_file_message(
        const std::string& receiver_id,
        bool is_group,
        const std::string& file_path);
    void send_text_with_file(
        const std::string& receiver_id,
        bool is_group,
        const std::string& text,
        const std::string& file_path);
    std::vector<ChatMessage> get_conversation_messages(const std::string& conversation_id, int limit = 50);

/* ---------- Print ---------- */
    void print_friends();
    void print_groups();


private:
    // 扔进去全存
    void store_relation_network_data(const json& relation_data);
};