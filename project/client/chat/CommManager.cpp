#include "../include//CommManager.hpp"
#include "../include/TopClient.hpp"
#include "../include/TcpClient.hpp"
#include "../../global/include/threadpool.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/logging.hpp"

CommManager::CommManager(TopClient* client) : top_client(client) {
    clients[0] = top_client->message_client;
    clients[1] = top_client->command_client;
    clients[2] = top_client->data_client;
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
    clients[idx]->socket->send_protocol(proto);
}

auto CommManager::send_async(int idx, const std::string& proto) {
    return top_client->pool->submit([this, idx, proto](){
        return clients[idx]->socket->send_protocol(proto);
    });
}

/* ---------- Handlers ---------- */

void CommManager::handle_recv_message(const ChatMessage& msg) {}

CommandRequest CommManager::handle_receive_command() {
    std::string proto;
    clients[1]->socket->receive_protocol(proto);
    return get_command_request(proto);
}

void CommManager::handle_send_command(Action action, const std::string& sender, std::initializer_list<std::string> args) {
    std::string env_out = create_command_string(action, sender, args);
    send(1, env_out);
}

void CommManager::handle_send_id() {
    for (int i=0; i<3; ++i) {
        auto env_out = create_command_string(
            Action::Remember_Connection, user_ID, {std::to_string(i)});
        send(i, env_out);
    }
    log_info("Sent user ID to all servers");
}

/* ---------- Print ---------- */