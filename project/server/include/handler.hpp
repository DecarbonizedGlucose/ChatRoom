#pragma once

#include <string>
#include <nlohmann/json.hpp>

class Dispatcher;
class TcpServer;
class TcpServerConnection;
class ChatMessage;
class CommandRequest;
class FileChunk;
class SyncItem;
class OfflineMessages;
enum class Action;

using json = nlohmann::json;

class Handler {
public:
    Dispatcher* disp = nullptr;
    Handler(Dispatcher* dispatcher);
};

/* -------------- Message -------------- */

class MessageHandler : public Handler {
public:
    MessageHandler(Dispatcher* dispatcher);

    void handle_recv(const ChatMessage& message, const std::string& ostr);
    void handle_send(TcpServerConnection* conn);
};

/* -------------- Command -------------- */

class CommandHandler : public Handler {
public:
    CommandHandler(Dispatcher* dispatcher);
    friend class Dispatcher;

    void handle_recv(
        TcpServerConnection* conn,
        const CommandRequest& command,
        const std::string& ostr);
    void handle_send(TcpServerConnection* conn);

private:
    void handle_sign_in(
        TcpServerConnection* conn,
        const std::string& subj,
        const std::string& password);
    void handle_sign_out(const std::string& user_ID);
    void handle_register(
        TcpServerConnection* conn,
        const std::string& email,
        std::string& user_ID,
        std::string& user_password);
    void handle_send_veri_code(
        TcpServerConnection* conn,
        std::string subj);
    void handle_find_password();
    void handle_change_password();
    void handle_change_username();
    void handle_authentication(
        TcpServerConnection* conn,
        const std::string& email,
        const std::string& veri_code);
    void handle_accept_friend_request();
    void handle_refuse_friend_request();
    void handle_accept_group_request();
    void handle_refuse_group_request();
    void handle_add_friend();
    void handle_remove_friend();
    void handle_search_person(TcpServerConnection* conn, const std::string& searched_ID);
    void handle_create_group();
    void handle_join_group();
    void handle_leave_group();
    void handle_disband_group();
    void handle_invite_to_group();
    void handle_remove_from_group();
    void handle_search_group();
    void handle_add_admin();
    void handle_remove_admin();
    void handle_update_relation_net(const std::string& user_ID);
    void handle_download_file();
    void handle_remember_connection(
        TcpServerConnection* conn,
        const std::string& user_ID,
        int server_index);
    void handle_online_init(const std::string& user_ID);

    // 非直接指令驱动的业务逻辑
    void handle_post_relation_net(const std::string& user_ID, json* relation_data);
    void handle_post_friends_status(const std::string& user_ID, const json& friends);
    void handle_post_offline_messages(const std::string& user_ID, const json& relation_data);
    void handle_post_unordered_noti_and_req(const std::string& user_ID, const json& relation_data);
    void handle_notify_friends_online(const std::string& user_ID, const json& friends);
};

/* -------------- Data -------------- */

class FileHandler : public Handler {
public:
    FileHandler(Dispatcher* dispatcher);

    void handle_recv(const FileChunk& file_chunk, const std::string& ostr);
    void handle_send(TcpServerConnection* conn);
};

class SyncHandler : public Handler {
public:
    SyncHandler(Dispatcher* dispatcher);

    void handle_send(TcpServerConnection* conn);
};

class OfflineMessageHandler : public Handler {
public:
    OfflineMessageHandler(Dispatcher* dispatcher);

    void handle_send(TcpServerConnection* conn);
};