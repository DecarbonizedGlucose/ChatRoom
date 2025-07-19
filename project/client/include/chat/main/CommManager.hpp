#pragma once

#include <string>
#include <initializer_list>

class TopClient;
class TcpClient;
enum class Action;

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
    void handle_send_command(Action action, const std::string& sender, std::initializer_list<std::string> args);
    // data
};