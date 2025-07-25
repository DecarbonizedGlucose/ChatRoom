#pragma once
#include <string>
#include <initializer_list>
#include "../../global/abstract/datatypes.hpp"

class TopClient;
class TcpClient;

class CommManager {
public:
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

/* ---------- Handlers ---------- */
    void handle_recv_message(const ChatMessage& msg);

    // message 其实消息也不好写
    CommandRequest handle_receive_command();
    void handle_send_command(Action action, const std::string& sender, std::initializer_list<std::string> args);
    // data

    void handle_send_id();

/* ---------- Print ---------- */

};