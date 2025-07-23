#include "../../include/chat/main/CommManager.hpp"
#include <google/protobuf/message.h>
#include "../../include/TopClient.hpp"
#include "../../include/TcpClient.hpp"
#include "../../../global/abstract/envelope.pb.h"
#include "../../../global/abstract/message.pb.h"
#include "../../../global/abstract/command.pb.h"
#include "../../../global/abstract/data.pb.h"
#include "../../../global/include/threadpool.hpp"

CommManager::CommManager(TopClient* client) : top_client(client) {
    clients[0] = top_client->message_client;
    clients[1] = top_client->command_client;
    clients[2] = top_client->data_client;
}

/* ---------- Input & Output ---------- */

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

CommandRequest CommManager::handle_receive_command() {
    Envelope env;
    std::string proto;
    clients[1]->socket->receive_protocol(proto);
    env.ParseFromString(proto);
    CommandRequest cmd;
    env.payload().UnpackTo(&cmd);
    return cmd;
}

void CommManager::handle_send_command(Action action, const std::string& sender, std::initializer_list<std::string> args) {
    CommandRequest cmd;
    cmd.set_action(static_cast<int>(action));
    cmd.set_sender(sender);
    for (const auto& arg : args) {
        cmd.add_args(arg);
    }
    google::protobuf::Any any;
    any.PackFrom(cmd);
    Envelope env;
    env.set_user_id(""); // 保留接口。这里可以写email
    env.mutable_payload()->PackFrom(any);
    std::string env_out;
    env.SerializeToString(&env_out);
    send(1, env_out);
}
