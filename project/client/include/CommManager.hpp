#pragma once
#include <string>
#include <initializer_list>
#include "../../global/abstract/datatypes.hpp"

class TopClient;
class TcpClient;
class SQLiteController;

class CommManager {
public:
    bool* cont = nullptr;
    SQLiteController* sqlite_con = nullptr;

    CommManager(TopClient* client);

    TopClient* top_client;
    TcpClient* clients[3];

    std::string user_ID;
    std::string password_hash;
    std::string email;

/* ---------- Pure Input & Output ---------- */
    std::string read(int idx);
    auto read_async(int idx);
    void send(int idx, const std::string& proto);
    auto send_async(int idx, const std::string& proto);
    std::string read_nb(int idx);
    void send_nb(int idx, const std::string& proto);

/* ---------- Handlers ---------- */
    ChatMessage handle_receive_message(bool nb = true);
    void handle_send_message(const std::string& sender, const std::string& receiver,
                             bool is_group_msg, const std::string& text,
                             bool pin = false, const std::string& file_name = "",
                             std::size_t file_size = 0, const std::string& file_hash = "",
                             bool nb = true);
    void handle_manage_message(const ChatMessage& msg);

    CommandRequest handle_receive_command(bool nb = true);
    void handle_send_command(Action action, const std::string& sender, std::initializer_list<std::string> args, bool nb = true);
    void handle_manage_command(const CommandRequest& cmd);

    void handle_send_id();

/* ---------- Print ---------- */

};