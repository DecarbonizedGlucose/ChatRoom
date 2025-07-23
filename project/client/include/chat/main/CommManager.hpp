#pragma once
#include <string>
#include <initializer_list>
#include "../../../../global/abstract/envelope.pb.h"
#include "../../../../global/abstract/command.pb.h"
#include "../../../../global/abstract/message.pb.h"
#include "../../../../global/abstract/data.pb.h"
#include "../../../../global/include/action.hpp"

class TopClient;
class TcpClient;

class CommManager {
private:
    TopClient* top_client;
    TcpClient* clients[3];

public:
    CommManager(TopClient* client);

/* ---------- Input & Output ---------- */
    std::string read(int idx);
    auto read_async(int idx);
    void send(int idx, const std::string& proto);
    auto send_async(int idx, const std::string& proto);

/* ---------- Handlers ---------- */
    // message 其实消息也不好写
    CommandRequest handle_receive_command();
    void handle_send_command(Action action, const std::string& sender, std::initializer_list<std::string> args);
    // data
};