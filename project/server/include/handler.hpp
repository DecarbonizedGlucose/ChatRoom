#pragma once

#include <string>

class Dispatcher;
class TcpServer;
class TcpServerConnection;
class ChatMessage;
class CommandRequest;
class FileChunk;
class SyncItem;
class OfflineMessages;
enum class Action;

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
    void handle_send(const TcpServerConnection* conn);
};

/* -------------- Command -------------- */

class CommandHandler : public Handler {
public:
    CommandHandler(Dispatcher* dispatcher);

    void handle_recv(
        const TcpServerConnection* conn,
        const CommandRequest& command,
        const std::string& ostr);
    void handle_send(const TcpServerConnection* conn);

private:
    void handle_sign_in();
    void handle_sign_out();
    void handle_register(
        const TcpServerConnection* conn,
        const std::string& email,
        std::string& user_ID,
        std::string& user_password);
    void handle_send_veri_code(
        const TcpServerConnection* conn,
        std::string subj);
    void handle_find_password();
    void handle_change_password();
    void handle_change_username();
    void handle_authentication(
        const TcpServerConnection* conn,
        const std::string& email,
        const std::string& veri_code
    );
    void handle_add_friend();
    void handle_remove_friend();
    void handle_search_person();
    void handle_create_group();
    void handle_join_group();
    void handle_leave_group();
    void handle_disband_group();
    void handle_invite_to_group();
    void handle_remove_from_group();
    void handle_search_group();
    void handle_add_admin();
    void handle_remove_admin();
    void handle_get_relation_net();
    void handle_download_file();
};

/* -------------- Data -------------- */

class FileHandler : public Handler {
public:
    FileHandler(Dispatcher* dispatcher);

    void handle_recv(const FileChunk& file_chunk, const std::string& ostr);
    void handle_send(const FileChunk& file_chunk, const std::string& ostr);
};

class SyncHandler : public Handler {
public:
    SyncHandler(Dispatcher* dispatcher);

    void handle_send(const SyncItem& sync_item, const std::string& ostr);
};

class OfflineMessageHandler : public Handler {
public:
    OfflineMessageHandler(Dispatcher* dispatcher);

    void handle_recv(const OfflineMessages& offline_messages, const std::string& ostr);
};

/* ---------- proto简单获取 ---------- */

CommandRequest create_command_request(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args);

std::string create_proto_cmd(
    Action action,
    /* const std::string sender, */
    std::initializer_list<std::string> args);
